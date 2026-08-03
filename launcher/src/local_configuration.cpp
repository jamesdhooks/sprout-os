#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <yyjson.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace sprout::launcher {
std::string_view setup_step_name(SetupStep step) {
  switch (step) {
    case SetupStep::Welcome:
      return "welcome";
    case SetupStep::Locale:
      return "locale";
    case SetupStep::Network:
      return "network";
    case SetupStep::Parent:
      return "parent";
    case SetupStep::ParentPin:
      return "parent-pin";
    case SetupStep::Child:
      return "child";
    case SetupStep::Avatars:
      return "avatars";
    case SetupStep::Library:
      return "library";
    case SetupStep::ChildDefaults:
      return "child-defaults";
    case SetupStep::Connectors:
      return "connectors";
    case SetupStep::Review:
      return "review";
    case SetupStep::Complete:
      return "complete";
  }
  throw std::runtime_error("Unsupported setup step");
}

namespace {

constexpr std::string_view kActiveFilename = "sprout.json";
constexpr std::string_view kLastKnownGoodFilename = "sprout.last-good.json";

SetupStep parse_step(std::string_view value) {
  for (const SetupStep step :
       {SetupStep::Welcome, SetupStep::Locale, SetupStep::Network,
        SetupStep::Parent, SetupStep::ParentPin, SetupStep::Child,
        SetupStep::Avatars, SetupStep::Library, SetupStep::ChildDefaults,
        SetupStep::Connectors, SetupStep::Review, SetupStep::Complete}) {
    if (setup_step_name(step) == value) {
      return step;
    }
  }
  throw std::runtime_error("Configuration contains an unsupported setup step");
}

void validate_locale(const LocaleOverrides& locale) {
  for (const auto* value : {&locale.language, &locale.region, &locale.time_zone}) {
    if (value->has_value() && value->value().empty()) {
      throw std::invalid_argument("Locale overrides cannot be empty strings");
    }
  }
}

yyjson_mut_val* write_locale(yyjson_mut_doc* document,
                             const LocaleOverrides& locale) {
  yyjson_mut_val* object = yyjson_mut_obj(document);
  const auto add = [&](const char* key, const std::optional<std::string>& value) {
    if (value.has_value()) {
      yyjson_mut_obj_add_strcpy(document, object, key, value->c_str());
    } else {
      yyjson_mut_obj_add_null(document, object, key);
    }
  };
  add("language", locale.language);
  add("region", locale.region);
  add("timeZone", locale.time_zone);
  return object;
}

std::string serialize(const LocalConfiguration& configuration) {
  yyjson_mut_doc* document = yyjson_mut_doc_new(nullptr);
  if (document == nullptr) {
    throw std::runtime_error("Could not allocate configuration document");
  }

  yyjson_mut_val* root = yyjson_mut_obj(document);
  yyjson_mut_doc_set_root(document, root);
  yyjson_mut_obj_add_uint(document, root, "schemaVersion",
                         configuration.schema_version);
  yyjson_mut_obj_add_uint(document, root, "revision", configuration.revision);
  yyjson_mut_obj_add_strcpy(document, root, "nextSetupStep",
                           std::string(setup_step_name(
                               configuration.next_setup_step)).c_str());

  yyjson_mut_val* household = yyjson_mut_obj(document);
  yyjson_mut_obj_add_strcpy(document, household, "id",
                           configuration.household_id.c_str());
  yyjson_mut_obj_add_val(document, household, "locale",
                         write_locale(document, configuration.household_locale));
  yyjson_mut_obj_add_val(document, root, "household", household);

  yyjson_mut_val* device = yyjson_mut_obj(document);
  yyjson_mut_obj_add_strcpy(document, device, "id", configuration.device_id.c_str());
  yyjson_mut_obj_add_bool(document, device, "offlineSetup",
                         configuration.offline_setup);
  yyjson_mut_obj_add_val(document, device, "locale",
                         write_locale(document, configuration.device_locale));
  yyjson_mut_obj_add_val(document, root, "device", device);

  yyjson_mut_val* profiles = yyjson_mut_arr(document);
  for (const auto& [id, locale] : configuration.profile_locales) {
    yyjson_mut_val* profile = yyjson_mut_obj(document);
    yyjson_mut_obj_add_strcpy(document, profile, "id", id.c_str());
    yyjson_mut_obj_add_val(document, profile, "locale",
                           write_locale(document, locale));
    yyjson_mut_arr_add_val(profiles, profile);
  }
  yyjson_mut_obj_add_val(document, root, "profiles", profiles);

  if (configuration.parent_credential_ref.has_value()) {
    yyjson_mut_obj_add_strcpy(document, root, "parentCredentialRef",
                             configuration.parent_credential_ref->c_str());
  } else {
    yyjson_mut_obj_add_null(document, root, "parentCredentialRef");
  }

  size_t length = 0;
  char* encoded = yyjson_mut_write(document, YYJSON_WRITE_PRETTY, &length);
  if (encoded == nullptr) {
    yyjson_mut_doc_free(document);
    throw std::runtime_error("Could not encode local configuration");
  }
  std::string result(encoded, length);
  free(encoded);
  yyjson_mut_doc_free(document);
  result.push_back('\n');
  return result;
}

void validate_keys(yyjson_val* object,
                   std::initializer_list<std::string_view> allowed) {
  if (!yyjson_is_obj(object)) {
    throw std::runtime_error("Configuration field should be an object");
  }
  const std::set<std::string_view> allowed_keys(allowed);
  std::set<std::string> observed;
  yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string name = yyjson_get_str(key);
    if (allowed_keys.find(name) == allowed_keys.end() ||
        !observed.insert(name).second) {
      throw std::runtime_error("Configuration contains an unknown or duplicate field");
    }
  }
}

yyjson_val* required(yyjson_val* object, const char* key) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (value == nullptr) {
    throw std::runtime_error(std::string("Configuration is missing field: ") + key);
  }
  return value;
}

std::string read_string(yyjson_val* object, const char* key) {
  yyjson_val* value = required(object, key);
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
    throw std::runtime_error(std::string("Configuration field should be text: ") + key);
  }
  return yyjson_get_str(value);
}

std::optional<std::string> read_optional_string(yyjson_val* object,
                                                const char* key) {
  yyjson_val* value = required(object, key);
  if (yyjson_is_null(value)) {
    return std::nullopt;
  }
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
    throw std::runtime_error(std::string("Configuration field should be text or null: ") +
                             key);
  }
  return std::string(yyjson_get_str(value));
}

LocaleOverrides read_locale(yyjson_val* value) {
  validate_keys(value, {"language", "region", "timeZone"});
  return LocaleOverrides{
      .language = read_optional_string(value, "language"),
      .region = read_optional_string(value, "region"),
      .time_zone = read_optional_string(value, "timeZone"),
  };
}

LocalConfiguration parse(std::string_view encoded) {
  yyjson_read_err error{};
  std::string buffer(encoded);
  yyjson_doc* document = yyjson_read_opts(buffer.data(), buffer.size(),
                                          YYJSON_READ_NOFLAG, nullptr, &error);
  if (document == nullptr) {
    throw std::runtime_error(std::string("Invalid configuration JSON: ") + error.msg);
  }

  try {
    yyjson_val* root = yyjson_doc_get_root(document);
    validate_keys(root, {"schemaVersion", "revision", "nextSetupStep", "household",
                         "device", "profiles", "parentCredentialRef"});
    yyjson_val* schema = required(root, "schemaVersion");
    if (!yyjson_is_uint(schema) ||
        yyjson_get_uint(schema) != kLocalConfigurationSchemaVersion) {
      throw std::runtime_error("Unsupported local configuration schema version");
    }
    yyjson_val* revision = required(root, "revision");
    if (!yyjson_is_uint(revision) || yyjson_get_uint(revision) == 0) {
      throw std::runtime_error("Configuration revision must be a positive integer");
    }

    yyjson_val* household = required(root, "household");
    validate_keys(household, {"id", "locale"});
    yyjson_val* device = required(root, "device");
    validate_keys(device, {"id", "offlineSetup", "locale"});
    yyjson_val* offline = required(device, "offlineSetup");
    if (!yyjson_is_bool(offline)) {
      throw std::runtime_error("offlineSetup should be a boolean");
    }

    LocalConfiguration configuration{
        .schema_version = kLocalConfigurationSchemaVersion,
        .revision = yyjson_get_uint(revision),
        .next_setup_step = parse_step(read_string(root, "nextSetupStep")),
        .household_id = read_string(household, "id"),
        .device_id = read_string(device, "id"),
        .offline_setup = yyjson_get_bool(offline),
        .household_locale = read_locale(required(household, "locale")),
        .device_locale = read_locale(required(device, "locale")),
        .profile_locales = {},
        .parent_credential_ref = read_optional_string(root, "parentCredentialRef"),
    };

    yyjson_val* profiles = required(root, "profiles");
    if (!yyjson_is_arr(profiles)) {
      throw std::runtime_error("profiles should be an array");
    }
    size_t index = 0;
    size_t maximum = 0;
    yyjson_val* profile = nullptr;
    yyjson_arr_foreach(profiles, index, maximum, profile) {
      validate_keys(profile, {"id", "locale"});
      const std::string id = read_string(profile, "id");
      if (!configuration.profile_locales
               .emplace(id, read_locale(required(profile, "locale")))
               .second) {
        throw std::runtime_error("Configuration contains a duplicate profile override");
      }
    }
    validate_configuration(configuration);
    yyjson_doc_free(document);
    return configuration;
  } catch (...) {
    yyjson_doc_free(document);
    throw;
  }
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Could not open configuration file");
  }
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
  FILE* file = nullptr;
#ifdef _WIN32
  if (_wfopen_s(&file, path.c_str(), L"wb") != 0) {
#else
  file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
#endif
    throw std::runtime_error("Could not create pending configuration file");
  }

  const bool wrote =
      std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
  const bool flushed = std::fflush(file) == 0;
  bool synchronized = false;
#ifdef _WIN32
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
  synchronized = handle != INVALID_HANDLE_VALUE && FlushFileBuffers(handle) != 0;
#else
  synchronized = fsync(fileno(file)) == 0;
#endif
  const bool closed = std::fclose(file) == 0;
  if (!wrote || !flushed || !synchronized || !closed) {
    throw std::runtime_error("Could not durably write pending configuration file");
  }
}

void replace_file(const std::filesystem::path& pending,
                  const std::filesystem::path& destination) {
#ifdef _WIN32
  if (!MoveFileExW(pending.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error("Could not atomically activate configuration file");
  }
#else
  if (std::rename(pending.c_str(), destination.c_str()) != 0) {
    throw std::runtime_error("Could not atomically activate configuration file");
  }
  const int directory = open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  if (directory >= 0) {
    fsync(directory);
    close(directory);
  }
#endif
}

void move_new_file(const std::filesystem::path& source,
                   const std::filesystem::path& destination) {
#ifdef _WIN32
  if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error(
        "Could not quarantine active launcher configuration");
  }
#else
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0U)
#endif
  if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD,
              destination.c_str(), RENAME_NOREPLACE) != 0) {
    throw std::runtime_error(
        "Could not quarantine active launcher configuration");
  }
  const int directory =
      open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  if (directory >= 0) {
    (void)fsync(directory);
    close(directory);
  }
#endif
}

void atomic_write(const std::filesystem::path& destination,
                  std::string_view contents) {
  std::filesystem::path pending = destination;
  pending += ".pending";
  try {
    write_file(pending, contents);
    replace_file(pending, destination);
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(pending, ignored);
    throw;
  }
}

}  // namespace

ResolvedLocale resolve_locale(const ResolvedLocale& platform_defaults,
                              const LocalConfiguration& configuration,
                              const std::optional<std::string>& profile_id) {
  ResolvedLocale result = platform_defaults;
  const auto apply = [&](const LocaleOverrides& overrides) {
    if (overrides.language.has_value()) {
      result.language = *overrides.language;
    }
    if (overrides.region.has_value()) {
      result.region = *overrides.region;
    }
    if (overrides.time_zone.has_value()) {
      result.time_zone = *overrides.time_zone;
    }
  };
  apply(configuration.household_locale);
  apply(configuration.device_locale);
  if (profile_id.has_value()) {
    const auto profile = configuration.profile_locales.find(*profile_id);
    if (profile != configuration.profile_locales.end()) {
      apply(profile->second);
    }
  }
  return result;
}

void validate_configuration(const LocalConfiguration& configuration) {
  if (configuration.schema_version != kLocalConfigurationSchemaVersion) {
    throw std::invalid_argument("Unsupported local configuration schema version");
  }
  if (configuration.household_id.empty() || configuration.device_id.empty()) {
    throw std::invalid_argument("Household and device IDs are required");
  }
  validate_locale(configuration.household_locale);
  validate_locale(configuration.device_locale);
  for (const auto& [id, locale] : configuration.profile_locales) {
    if (id.empty()) {
      throw std::invalid_argument("Profile override ID is required");
    }
    validate_locale(locale);
  }
  if (configuration.parent_credential_ref.has_value() &&
      !starts_with(*configuration.parent_credential_ref, "secret:")) {
    throw std::invalid_argument("Parent credential must be an opaque secret: reference");
  }
}

ConfigurationStore::ConfigurationStore(std::filesystem::path configuration_directory)
    : directory_(std::move(configuration_directory)),
      active_path_(directory_ / kActiveFilename),
      last_known_good_path_(directory_ / kLastKnownGoodFilename) {}

bool ConfigurationStore::has_active() const {
  return std::filesystem::is_regular_file(active_path_);
}

bool ConfigurationStore::has_last_known_good() const {
  return std::filesystem::is_regular_file(last_known_good_path_);
}

LocalConfiguration ConfigurationStore::load_active() const {
  return parse(read_file(active_path_));
}

LocalConfiguration ConfigurationStore::load_last_known_good() const {
  if (!has_last_known_good()) {
    throw std::runtime_error("No last-known-good configuration is available");
  }
  return parse(read_file(last_known_good_path_));
}

LocalConfiguration ConfigurationStore::save(LocalConfiguration configuration) {
  validate_configuration(configuration);
  std::filesystem::create_directories(directory_);
  if (has_active()) {
    const std::string active_contents = read_file(active_path_);
    const LocalConfiguration active = parse(active_contents);
    if (configuration.revision != active.revision) {
      throw std::runtime_error("Configuration changed after it was loaded");
    }
    if (active.revision == std::numeric_limits<std::uint64_t>::max()) {
      throw std::runtime_error("Configuration revision is exhausted");
    }
    configuration.revision = active.revision + 1;
    atomic_write(last_known_good_path_, active_contents);
  } else {
    if (configuration.revision != 0) {
      throw std::runtime_error("New configuration must start at revision zero");
    }
    configuration.revision = 1;
  }
  atomic_write(active_path_, serialize(configuration));
  return configuration;
}

LocalConfiguration ConfigurationStore::restore_last_known_good() {
  if (!has_last_known_good()) {
    throw std::runtime_error("No last-known-good configuration is available");
  }
  const std::string contents = read_file(last_known_good_path_);
  LocalConfiguration configuration = parse(contents);
  atomic_write(active_path_, contents);
  return configuration;
}

std::optional<std::filesystem::path> ConfigurationStore::quarantine_active(
    std::uint64_t startup_attempt_id) {
  if (startup_attempt_id == 0) {
    throw std::invalid_argument("Startup attempt identity is required");
  }
  std::error_code error;
  const auto active_status = std::filesystem::symlink_status(active_path_, error);
  if (active_status.type() == std::filesystem::file_type::not_found) {
    return std::nullopt;
  }
  if (error || active_status.type() != std::filesystem::file_type::regular) {
    throw std::runtime_error(
        "Active launcher configuration is missing, linked, or irregular");
  }

  const auto recovery_directory = directory_ / "recovery";
  error.clear();
  auto recovery_status =
      std::filesystem::symlink_status(recovery_directory, error);
  if (recovery_status.type() == std::filesystem::file_type::not_found) {
    error.clear();
    if (!std::filesystem::create_directory(recovery_directory, error) || error) {
      throw std::runtime_error("Could not create configuration recovery directory");
    }
    recovery_status =
        std::filesystem::symlink_status(recovery_directory, error);
  }
  if (error || recovery_status.type() != std::filesystem::file_type::directory) {
    throw std::runtime_error(
        "Configuration recovery path is linked or not a directory");
  }

  const auto destination =
      recovery_directory /
      ("sprout.failed-startup-" + std::to_string(startup_attempt_id) + ".json");
  error.clear();
  const auto destination_status =
      std::filesystem::symlink_status(destination, error);
  if (destination_status.type() != std::filesystem::file_type::not_found) {
    throw std::runtime_error("Configuration recovery destination already exists");
  }
  move_new_file(active_path_, destination);
  return destination;
}

const std::filesystem::path& ConfigurationStore::active_path() const noexcept {
  return active_path_;
}

const std::filesystem::path& ConfigurationStore::last_known_good_path() const noexcept {
  return last_known_good_path_;
}

}  // namespace sprout::launcher
