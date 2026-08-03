#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <argon2.h>
#include <blake2.h>
#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sprout::launcher {
namespace {

constexpr std::uint32_t kArgonTimeCost = 2;
constexpr std::uint32_t kArgonMemoryKiB = 19U * 1024U;
constexpr std::uint32_t kArgonLanes = 1;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kNonceBytes = 16;
constexpr std::size_t kMacBytes = 32;

class Statement {
 public:
  Statement(sqlite3* database, std::string_view sql) {
    const std::string statement_sql(sql);
    if (sqlite3_prepare_v2(database, statement_sql.c_str(), -1, &statement_, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }
  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  sqlite3_stmt* get() const noexcept { return statement_; }

 private:
  sqlite3_stmt* statement_{nullptr};
};

void execute(sqlite3* database, std::string_view sql) {
  char* error = nullptr;
  const std::string statement_sql(sql);
  if (sqlite3_exec(database, statement_sql.c_str(), nullptr, nullptr, &error) ==
      SQLITE_OK) {
    return;
  }
  const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
  sqlite3_free(error);
  throw std::runtime_error(message);
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
  if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    throw std::runtime_error("Could not bind parent-access text");
  }
}

void bind_blob(sqlite3_stmt* statement, int index, const void* data,
               std::size_t size) {
  if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      sqlite3_bind_blob(statement, index, data, static_cast<int>(size),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error("Could not bind parent-access bytes");
  }
}

void step_done(sqlite3* database, sqlite3_stmt* statement) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

void secure_clear(std::string& value) noexcept {
  volatile char* bytes = value.empty() ? nullptr : value.data();
  for (std::size_t index = 0; index < value.size(); ++index) {
    bytes[index] = 0;
  }
  value.clear();
}

void validate_credential_ref(std::string_view reference) {
  if (!starts_with(reference, "secret:") || reference.size() > 96 ||
      !std::all_of(reference.begin() + 7, reference.end(), [](unsigned char value) {
        return std::isalnum(value) != 0 || value == '-' || value == '_';
      })) {
    throw std::invalid_argument("Credential reference must use a safe secret: identifier");
  }
}

void validate_pin(std::string_view pin) {
  if (pin.size() < 4 || pin.size() > 8 ||
      !std::all_of(pin.begin(), pin.end(), [](unsigned char value) {
        return std::isdigit(value) != 0;
      })) {
    throw std::invalid_argument("Parent PIN must contain 4 to 8 digits");
  }
}

void validate_local_date(std::string_view value) {
  if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
    throw std::invalid_argument("Local date must use YYYY-MM-DD");
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (index != 4 && index != 7 &&
        std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
      throw std::invalid_argument("Local date must use YYYY-MM-DD");
    }
  }
  const int year = std::stoi(std::string(value.substr(0, 4)));
  const unsigned month = static_cast<unsigned>(std::stoi(std::string(value.substr(5, 2))));
  const unsigned day = static_cast<unsigned>(std::stoi(std::string(value.substr(8, 2))));
  static constexpr std::array<unsigned, 12> days_per_month{
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 1 || month < 1 || month > days_per_month.size()) {
    throw std::invalid_argument("Local date is not a calendar date");
  }
  unsigned maximum_day = days_per_month[month - 1];
  const bool leap_year =
      year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (month == 2 && leap_year) {
    ++maximum_day;
  }
  if (day < 1 || day > maximum_day) {
    throw std::invalid_argument("Local date is not a calendar date");
  }
}

void random_bytes(void* output, std::size_t size) {
#ifdef _WIN32
  if (size > std::numeric_limits<ULONG>::max() ||
      BCryptGenRandom(nullptr, static_cast<PUCHAR>(output), static_cast<ULONG>(size),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
    throw std::runtime_error("Secure random generation failed");
  }
#else
  const int file = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (file < 0) {
    throw std::runtime_error("Secure random source could not be opened");
  }
  std::size_t offset = 0;
  while (offset < size) {
    const auto count = read(file, static_cast<unsigned char*>(output) + offset,
                            size - offset);
    if (count <= 0) {
      close(file);
      throw std::runtime_error("Secure random source could not be read");
    }
    offset += static_cast<std::size_t>(count);
  }
  close(file);
#endif
}

std::array<unsigned char, kKeyBytes> load_or_create_key(
    const std::filesystem::path& path) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::array<unsigned char, kKeyBytes> key{};
  if (std::filesystem::exists(path)) {
    std::ifstream stream(path, std::ios::binary);
    stream.read(reinterpret_cast<char*>(key.data()), key.size());
    if (!stream || stream.peek() != std::char_traits<char>::eof()) {
      throw std::runtime_error("Parent-access device key is malformed");
    }
    return key;
  }
  random_bytes(key.data(), key.size());
  auto pending = path;
  pending += ".pending";
  {
    std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(key.data()), key.size());
    if (!stream) {
      throw std::runtime_error("Parent-access device key could not be written");
    }
  }
  std::filesystem::permissions(
      pending, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
  std::filesystem::rename(pending, path);
  return key;
}

std::vector<unsigned char> grant_payload(const std::string& credential_ref,
                                         std::int64_t issued_at,
                                         std::int64_t last_observed,
                                         const std::string& local_date,
                                         const void* nonce, std::size_t nonce_size) {
  const std::string header = credential_ref + "\n" + std::to_string(issued_at) + "\n" +
                             std::to_string(last_observed) + "\n" + local_date + "\n";
  std::vector<unsigned char> payload(header.begin(), header.end());
  const auto* nonce_bytes = static_cast<const unsigned char*>(nonce);
  payload.insert(payload.end(), nonce_bytes, nonce_bytes + nonce_size);
  return payload;
}

std::array<unsigned char, kMacBytes> grant_mac(
    const std::array<unsigned char, kKeyBytes>& key,
    const std::vector<unsigned char>& payload) {
  std::array<unsigned char, kMacBytes> mac{};
  if (blake2b(mac.data(), mac.size(), payload.data(), payload.size(), key.data(),
              key.size()) != 0) {
    throw std::runtime_error("Could not authenticate parent-access grant");
  }
  return mac;
}

bool constant_equal(const unsigned char* left, const unsigned char* right,
                    std::size_t size) noexcept {
  unsigned char difference = 0;
  for (std::size_t index = 0; index < size; ++index) {
    difference |= left[index] ^ right[index];
  }
  return difference == 0;
}

}  // namespace

class ParentAccessStore::Impl {
 public:
  Impl(const std::filesystem::path& database_path,
      const std::filesystem::path& device_key_path)
      : key_(load_or_create_key(device_key_path)) {
    if (!database_path.parent_path().empty()) {
      std::filesystem::create_directories(database_path.parent_path());
    }
    const auto encoded = path_as_utf8(database_path);
    if (sqlite3_open_v2(encoded.c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
      const std::string message =
          database_ == nullptr ? "Could not open parent-access database"
                               : sqlite3_errmsg(database_);
      sqlite3_close(database_);
      database_ = nullptr;
      throw std::runtime_error(message);
    }
    try {
      sqlite3_busy_timeout(database_, 5000);
      execute(database_, "PRAGMA foreign_keys = ON");
      std::int64_t schema_version = 0;
      {
        Statement version(database_, "PRAGMA user_version");
        if (sqlite3_step(version.get()) != SQLITE_ROW) {
          throw std::runtime_error("Could not read parent-access schema version");
        }
        schema_version = sqlite3_column_int64(version.get(), 0);
      }
      if (schema_version > 1) {
        throw std::runtime_error(
            "Parent-access database is newer than this Sprout build");
      }
      execute(database_, R"sql(
        CREATE TABLE IF NOT EXISTS credentials (
          credential_ref TEXT PRIMARY KEY NOT NULL,
          encoded_hash TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS active_grant (
          singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
          credential_ref TEXT NOT NULL,
          issued_at INTEGER NOT NULL,
          last_observed INTEGER NOT NULL,
          local_date TEXT NOT NULL,
          nonce BLOB NOT NULL CHECK (length(nonce) = 16),
          mac BLOB NOT NULL CHECK (length(mac) = 32),
          FOREIGN KEY (credential_ref) REFERENCES credentials(credential_ref)
        );
      )sql");
      if (schema_version == 0) {
        execute(database_, "PRAGMA user_version = 1");
      }
    } catch (...) {
      sqlite3_close(database_);
      database_ = nullptr;
      throw;
    }
  }

  ~Impl() {
    volatile unsigned char* key_bytes = key_.data();
    for (std::size_t index = 0; index < key_.size(); ++index) {
      key_bytes[index] = 0;
    }
    sqlite3_close(database_);
  }

  sqlite3* database_{nullptr};
  std::array<unsigned char, kKeyBytes> key_{};
};

ParentAccessStore::ParentAccessStore(std::filesystem::path database_path,
                                     std::filesystem::path device_key_path)
    : impl_(std::make_unique<Impl>(database_path, device_key_path)) {}

ParentAccessStore::~ParentAccessStore() = default;
ParentAccessStore::ParentAccessStore(ParentAccessStore&&) noexcept = default;
ParentAccessStore& ParentAccessStore::operator=(ParentAccessStore&&) noexcept = default;

void ParentAccessStore::set_pin(const std::string& credential_ref, std::string pin) {
  validate_credential_ref(credential_ref);
  validate_pin(pin);
  std::array<unsigned char, kSaltBytes> salt{};
  random_bytes(salt.data(), salt.size());
  const auto encoded_size = argon2_encodedlen(kArgonTimeCost, kArgonMemoryKiB,
                                               kArgonLanes, kSaltBytes, kHashBytes,
                                               Argon2_id);
  std::vector<char> encoded(encoded_size);
  const int result = argon2id_hash_encoded(
      kArgonTimeCost, kArgonMemoryKiB, kArgonLanes, pin.data(), pin.size(), salt.data(),
      salt.size(), kHashBytes, encoded.data(), encoded.size());
  secure_clear(pin);
  if (result != ARGON2_OK) {
    throw std::runtime_error(argon2_error_message(result));
  }
  Statement statement(impl_->database_, R"sql(
    INSERT INTO credentials (credential_ref, encoded_hash) VALUES (?, ?)
    ON CONFLICT(credential_ref) DO UPDATE SET encoded_hash = excluded.encoded_hash
  )sql");
  bind_text(statement.get(), 1, credential_ref);
  bind_text(statement.get(), 2, encoded.data());
  execute(impl_->database_, "BEGIN IMMEDIATE");
  try {
    step_done(impl_->database_, statement.get());
    execute(impl_->database_, "DELETE FROM active_grant");
    execute(impl_->database_, "COMMIT");
  } catch (...) {
    execute(impl_->database_, "ROLLBACK");
    throw;
  }
}

bool ParentAccessStore::verify_pin(const std::string& credential_ref,
                                   std::string pin) const {
  validate_credential_ref(credential_ref);
  validate_pin(pin);
  Statement statement(impl_->database_,
                      "SELECT encoded_hash FROM credentials WHERE credential_ref = ?");
  bind_text(statement.get(), 1, credential_ref);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    secure_clear(pin);
    return false;
  }
  const auto* encoded = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0));
  if (encoded == nullptr) {
    secure_clear(pin);
    throw std::runtime_error("Parent credential hash is malformed");
  }
  const int result = argon2id_verify(encoded, pin.data(), pin.size());
  secure_clear(pin);
  return result == ARGON2_OK;
}

void ParentAccessStore::grant_until_end_of_day(
    const std::string& credential_ref, std::string pin,
    std::int64_t now_utc_seconds, const std::string& local_date) {
  validate_local_date(local_date);
  if (now_utc_seconds < 0 || !verify_pin(credential_ref, std::move(pin))) {
    throw std::invalid_argument("Parent authentication failed");
  }
  std::array<unsigned char, kNonceBytes> nonce{};
  random_bytes(nonce.data(), nonce.size());
  const auto payload = grant_payload(credential_ref, now_utc_seconds, now_utc_seconds,
                                     local_date, nonce.data(), nonce.size());
  const auto mac = grant_mac(impl_->key_, payload);
  Statement statement(impl_->database_, R"sql(
    INSERT OR REPLACE INTO active_grant
      (singleton, credential_ref, issued_at, last_observed, local_date, nonce, mac)
    VALUES (1, ?, ?, ?, ?, ?, ?)
  )sql");
  bind_text(statement.get(), 1, credential_ref);
  sqlite3_bind_int64(statement.get(), 2, now_utc_seconds);
  sqlite3_bind_int64(statement.get(), 3, now_utc_seconds);
  bind_text(statement.get(), 4, local_date);
  bind_blob(statement.get(), 5, nonce.data(), nonce.size());
  bind_blob(statement.get(), 6, mac.data(), mac.size());
  step_done(impl_->database_, statement.get());
}

bool ParentAccessStore::is_unlocked(std::int64_t now_utc_seconds,
                                    const std::string& local_date) {
  validate_local_date(local_date);
  std::string credential_ref;
  std::string granted_date;
  std::int64_t issued_at = 0;
  std::int64_t last_observed = 0;
  std::vector<unsigned char> nonce;
  std::vector<unsigned char> stored_mac;
  {
    Statement statement(impl_->database_, R"sql(
      SELECT credential_ref, issued_at, last_observed, local_date, nonce, mac
      FROM active_grant WHERE singleton = 1
    )sql");
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return false;
    }
    const auto* credential_text = sqlite3_column_text(statement.get(), 0);
    const auto* date_text = sqlite3_column_text(statement.get(), 3);
    const auto* nonce_data = static_cast<const unsigned char*>(
        sqlite3_column_blob(statement.get(), 4));
    const auto* mac_data = static_cast<const unsigned char*>(
        sqlite3_column_blob(statement.get(), 5));
    const int nonce_size = sqlite3_column_bytes(statement.get(), 4);
    const int mac_size = sqlite3_column_bytes(statement.get(), 5);
    if (credential_text == nullptr || date_text == nullptr || nonce_data == nullptr ||
        mac_data == nullptr || nonce_size != static_cast<int>(kNonceBytes) ||
        mac_size != static_cast<int>(kMacBytes)) {
      credential_ref.clear();
    } else {
      credential_ref = reinterpret_cast<const char*>(credential_text);
      granted_date = reinterpret_cast<const char*>(date_text);
      issued_at = sqlite3_column_int64(statement.get(), 1);
      last_observed = sqlite3_column_int64(statement.get(), 2);
      nonce.assign(nonce_data, nonce_data + nonce_size);
      stored_mac.assign(mac_data, mac_data + mac_size);
    }
  }
  if (credential_ref.empty()) {
    lock();
    return false;
  }
  const auto payload = grant_payload(credential_ref, issued_at, last_observed,
                                     granted_date, nonce.data(), nonce.size());
  const auto expected_mac = grant_mac(impl_->key_, payload);
  if (!constant_equal(expected_mac.data(), stored_mac.data(),
                      expected_mac.size()) ||
      now_utc_seconds < issued_at || now_utc_seconds < last_observed ||
      local_date != granted_date) {
    lock();
    return false;
  }

  const auto updated_payload = grant_payload(credential_ref, issued_at, now_utc_seconds,
                                             granted_date, nonce.data(), nonce.size());
  const auto updated_mac = grant_mac(impl_->key_, updated_payload);
  Statement update(impl_->database_,
                   "UPDATE active_grant SET last_observed = ?, mac = ? WHERE singleton = 1");
  sqlite3_bind_int64(update.get(), 1, now_utc_seconds);
  bind_blob(update.get(), 2, updated_mac.data(), updated_mac.size());
  step_done(impl_->database_, update.get());
  return true;
}

void ParentAccessStore::lock() {
  execute(impl_->database_, "DELETE FROM active_grant");
}

}  // namespace sprout::launcher
