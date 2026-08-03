#include "sprout/launcher/profile_archive.hpp"

#include "sprout/launcher/managed_profile_image.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <blake2.h>
#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace sprout::launcher {
namespace {

constexpr std::string_view kArchiveKind = "sprout-profile";
constexpr std::string_view kChecksumAlgorithm = "blake2b-256";
constexpr std::string_view kPayloadEncoding = "base64";
constexpr std::uintmax_t kMaximumArchiveBytes = 8U * 1024U * 1024U;
constexpr std::size_t kMaximumPayloadBytes = 6U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumPortraitBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumThumbnailBytes = 256U * 1024U;
constexpr std::array<unsigned char, 8> kPngSignature{
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

struct ParsedArchive {
  ProfileArchiveSummary summary;
  NewProfile profile;
  std::optional<std::uint32_t> daily_allowance_seconds;
  std::optional<std::vector<std::uint8_t>> portrait;
  std::optional<std::vector<std::uint8_t>> thumbnail;
  std::string payload_checksum;
};

void validate_keys(yyjson_val* object,
                   std::initializer_list<std::string_view> allowed) {
  if (!yyjson_is_obj(object)) {
    throw std::runtime_error("Profile archive field should be an object");
  }
  const std::set<std::string_view> allowed_keys(allowed);
  std::set<std::string> observed;
  yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string name = yyjson_get_str(key);
    if (allowed_keys.find(name) == allowed_keys.end() ||
        !observed.insert(name).second) {
      throw std::runtime_error(
          "Profile archive contains an unknown or duplicate field");
    }
  }
}

yyjson_val* required(yyjson_val* object, const char* key) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (value == nullptr) {
    throw std::runtime_error(std::string("Profile archive is missing field: ") +
                             key);
  }
  return value;
}

std::string read_string(yyjson_val* object, const char* key,
                        std::size_t maximum = 4'096) {
  yyjson_val* value = required(object, key);
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0 ||
      yyjson_get_len(value) > maximum) {
    throw std::runtime_error(std::string("Invalid profile archive text: ") + key);
  }
  return yyjson_get_str(value);
}

std::optional<std::string> read_optional_string(yyjson_val* object,
                                                const char* key) {
  yyjson_val* value = required(object, key);
  if (yyjson_is_null(value)) {
    return std::nullopt;
  }
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0 ||
      yyjson_get_len(value) > 4'096) {
    throw std::runtime_error(std::string("Invalid optional archive text: ") + key);
  }
  return std::string(yyjson_get_str(value));
}

void validate_portable_text(std::string_view value, std::string_view field) {
  if (value.empty() || value.size() > 4'096 ||
      std::any_of(value.begin(), value.end(), [](unsigned char character) {
        return character < 0x20 || character == 0x7f;
      })) {
    throw std::runtime_error(std::string("Invalid portable profile field: ") +
                             std::string(field));
  }
}

std::string role_name(ProfileRole role) {
  return role == ProfileRole::Parent ? "parent" : "child";
}

ProfileRole parse_role(std::string_view value) {
  if (value == "parent") {
    return ProfileRole::Parent;
  }
  if (value == "child") {
    return ProfileRole::Child;
  }
  throw std::runtime_error("Profile archive contains an unsupported role");
}

std::string base64_encode(const std::uint8_t* data, std::size_t size) {
  static constexpr char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((size + 2) / 3) * 4);
  for (std::size_t index = 0; index < size; index += 3) {
    const std::uint32_t first = data[index];
    const std::uint32_t second = index + 1 < size ? data[index + 1] : 0;
    const std::uint32_t third = index + 2 < size ? data[index + 2] : 0;
    const std::uint32_t value = (first << 16U) | (second << 8U) | third;
    result.push_back(alphabet[(value >> 18U) & 0x3fU]);
    result.push_back(alphabet[(value >> 12U) & 0x3fU]);
    result.push_back(index + 1 < size ? alphabet[(value >> 6U) & 0x3fU] : '=');
    result.push_back(index + 2 < size ? alphabet[value & 0x3fU] : '=');
  }
  return result;
}

std::string base64_encode(std::string_view value) {
  return base64_encode(reinterpret_cast<const std::uint8_t*>(value.data()),
                       value.size());
}

int base64_value(unsigned char character) noexcept {
  if (character >= 'A' && character <= 'Z') {
    return character - 'A';
  }
  if (character >= 'a' && character <= 'z') {
    return character - 'a' + 26;
  }
  if (character >= '0' && character <= '9') {
    return character - '0' + 52;
  }
  if (character == '+') {
    return 62;
  }
  if (character == '/') {
    return 63;
  }
  return -1;
}

std::vector<std::uint8_t> base64_decode(std::string_view encoded,
                                        std::size_t maximum_output) {
  if (encoded.empty() || encoded.size() % 4 != 0 ||
      encoded.size() / 4 * 3 > maximum_output + 2) {
    throw std::runtime_error("Profile archive contains invalid base64 size");
  }
  std::vector<std::uint8_t> result;
  result.reserve(encoded.size() / 4 * 3);
  for (std::size_t index = 0; index < encoded.size(); index += 4) {
    const bool final_group = index + 4 == encoded.size();
    const bool third_padding = encoded[index + 2] == '=';
    const bool fourth_padding = encoded[index + 3] == '=';
    if ((!final_group && (third_padding || fourth_padding)) ||
        (third_padding && !fourth_padding)) {
      throw std::runtime_error("Profile archive contains invalid base64 padding");
    }
    const int first = base64_value(static_cast<unsigned char>(encoded[index]));
    const int second = base64_value(static_cast<unsigned char>(encoded[index + 1]));
    const int third = third_padding
                          ? 0
                          : base64_value(
                                static_cast<unsigned char>(encoded[index + 2]));
    const int fourth = fourth_padding
                           ? 0
                           : base64_value(
                                 static_cast<unsigned char>(encoded[index + 3]));
    if (first < 0 || second < 0 || third < 0 || fourth < 0) {
      throw std::runtime_error("Profile archive contains invalid base64 data");
    }
    const std::uint32_t value =
        (static_cast<std::uint32_t>(first) << 18U) |
        (static_cast<std::uint32_t>(second) << 12U) |
        (static_cast<std::uint32_t>(third) << 6U) |
        static_cast<std::uint32_t>(fourth);
    result.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    if (!third_padding) {
      result.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    }
    if (!fourth_padding) {
      result.push_back(static_cast<std::uint8_t>(value & 0xffU));
    }
  }
  if (result.size() > maximum_output) {
    throw std::runtime_error("Profile archive decoded data is too large");
  }
  return result;
}

std::string digest_hex(std::string_view payload) {
  std::array<unsigned char, 32> digest{};
  if (blake2b(digest.data(), digest.size(), payload.data(), payload.size(), nullptr,
              0) != 0) {
    throw std::runtime_error("Could not checksum profile archive payload");
  }
  static constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(digest.size() * 2);
  for (const unsigned char byte : digest) {
    result.push_back(hex[byte >> 4U]);
    result.push_back(hex[byte & 0x0fU]);
  }
  return result;
}

bool valid_digest(std::string_view value) noexcept {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

std::vector<std::uint8_t> read_regular_file(const std::filesystem::path& path,
                                            std::uintmax_t maximum_size,
                                            const char* description) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  const auto size = std::filesystem::file_size(path, error);
  if (error || status.type() != std::filesystem::file_type::regular || size == 0 ||
      size > maximum_size) {
    throw std::runtime_error(std::string(description) +
                             " is missing, linked, empty, or too large");
  }
  std::ifstream stream(path, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  stream.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  if (!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error(std::string("Could not read ") + description);
  }
  return bytes;
}

std::string read_archive_file(const std::filesystem::path& path) {
  const auto bytes = read_regular_file(path, kMaximumArchiveBytes,
                                       "Profile archive");
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::uint32_t read_png_u32(const std::vector<std::uint8_t>& bytes,
                           std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

void validate_managed_png(const std::vector<std::uint8_t>& bytes,
                          std::uint32_t expected_size, const char* description) {
  if (bytes.size() < 24 ||
      !std::equal(kPngSignature.begin(), kPngSignature.end(), bytes.begin()) ||
      bytes[12] != 'I' || bytes[13] != 'H' || bytes[14] != 'D' ||
      bytes[15] != 'R' || read_png_u32(bytes, 16) != expected_size ||
      read_png_u32(bytes, 20) != expected_size) {
    throw std::runtime_error(std::string(description) +
                             " is not a normalized Sprout PNG");
  }
}

std::string serialize_payload(
    const ProfileRecord& profile,
    const std::optional<std::uint32_t>& daily_allowance,
    const std::optional<std::vector<std::uint8_t>>& portrait,
    const std::optional<std::vector<std::uint8_t>>& thumbnail) {
  yyjson_mut_doc* document = yyjson_mut_doc_new(nullptr);
  if (document == nullptr) {
    throw std::runtime_error("Could not allocate profile archive payload");
  }
  yyjson_mut_val* root = yyjson_mut_obj(document);
  yyjson_mut_doc_set_root(document, root);
  yyjson_mut_obj_add_uint(document, root, "schemaVersion",
                         kProfileArchiveSchemaVersion);
  yyjson_mut_val* value = yyjson_mut_obj(document);
  yyjson_mut_obj_add_strcpy(document, value, "id", profile.id.c_str());
  yyjson_mut_obj_add_strcpy(document, value, "displayName",
                           profile.display_name.c_str());
  yyjson_mut_obj_add_strcpy(document, value, "role",
                           role_name(profile.role).c_str());
  yyjson_mut_obj_add_strcpy(document, value, "saveNamespace",
                           profile.save_namespace.c_str());
  if (profile.content_policy_ref.has_value()) {
    yyjson_mut_obj_add_strcpy(document, value, "contentPolicyRef",
                             profile.content_policy_ref->c_str());
  } else {
    yyjson_mut_obj_add_null(document, value, "contentPolicyRef");
  }
  if (profile.time_policy_ref.has_value()) {
    yyjson_mut_obj_add_strcpy(document, value, "timePolicyRef",
                             profile.time_policy_ref->c_str());
  } else {
    yyjson_mut_obj_add_null(document, value, "timePolicyRef");
  }
  yyjson_mut_obj_add_strcpy(document, value, "preferencesJson",
                           profile.preferences_json.c_str());
  yyjson_mut_obj_add_strcpy(document, value, "backgroundRef",
                           profile.background_ref.c_str());
  yyjson_mut_val* avatar = yyjson_mut_obj(document);
  if (portrait.has_value() && thumbnail.has_value()) {
    const auto portrait_text = base64_encode(portrait->data(), portrait->size());
    const auto thumbnail_text = base64_encode(thumbnail->data(), thumbnail->size());
    yyjson_mut_obj_add_str(document, avatar, "kind", "managedPng");
    yyjson_mut_obj_add_strcpy(document, avatar, "portrait", portrait_text.c_str());
    yyjson_mut_obj_add_strcpy(document, avatar, "thumbnail", thumbnail_text.c_str());
  } else {
    yyjson_mut_obj_add_str(document, avatar, "kind", "builtin");
    yyjson_mut_obj_add_strcpy(document, avatar, "reference",
                             profile.avatar_ref.c_str());
  }
  yyjson_mut_obj_add_val(document, value, "avatar", avatar);
  yyjson_mut_obj_add_val(document, root, "profile", value);
  if (daily_allowance.has_value()) {
    yyjson_mut_obj_add_uint(document, root, "dailyAllowanceSeconds",
                           *daily_allowance);
  } else {
    yyjson_mut_obj_add_null(document, root, "dailyAllowanceSeconds");
  }

  size_t length = 0;
  char* encoded = yyjson_mut_write(document, YYJSON_WRITE_NOFLAG, &length);
  if (encoded == nullptr) {
    yyjson_mut_doc_free(document);
    throw std::runtime_error("Could not encode profile archive payload");
  }
  std::string result(encoded, length);
  free(encoded);
  yyjson_mut_doc_free(document);
  return result;
}

std::string serialize_archive(std::string_view payload,
                              const std::string& checksum) {
  const std::string encoded_payload = base64_encode(payload);
  yyjson_mut_doc* document = yyjson_mut_doc_new(nullptr);
  if (document == nullptr) {
    throw std::runtime_error("Could not allocate profile archive manifest");
  }
  yyjson_mut_val* root = yyjson_mut_obj(document);
  yyjson_mut_doc_set_root(document, root);
  yyjson_mut_obj_add_str(document, root, "archiveKind", kArchiveKind.data());
  yyjson_mut_obj_add_uint(document, root, "schemaVersion",
                         kProfileArchiveSchemaVersion);
  yyjson_mut_obj_add_str(document, root, "checksumAlgorithm",
                         kChecksumAlgorithm.data());
  yyjson_mut_obj_add_str(document, root, "payloadEncoding",
                         kPayloadEncoding.data());
  yyjson_mut_obj_add_strcpy(document, root, "payloadChecksum",
                           checksum.c_str());
  yyjson_mut_obj_add_strcpy(document, root, "payload", encoded_payload.c_str());
  size_t length = 0;
  char* encoded = yyjson_mut_write(document, YYJSON_WRITE_PRETTY, &length);
  if (encoded == nullptr) {
    yyjson_mut_doc_free(document);
    throw std::runtime_error("Could not encode profile archive manifest");
  }
  std::string result(encoded, length);
  free(encoded);
  yyjson_mut_doc_free(document);
  result.push_back('\n');
  return result;
}

yyjson_doc* parse_json(std::string& encoded, const char* description) {
  yyjson_read_err error{};
  yyjson_doc* document = yyjson_read_opts(
      encoded.data(), encoded.size(), YYJSON_READ_NOFLAG, nullptr, &error);
  if (document == nullptr) {
    throw std::runtime_error(std::string("Invalid ") + description + ": " +
                             error.msg);
  }
  return document;
}

ParsedArchive parse_archive(std::string encoded) {
  yyjson_doc* manifest_document = parse_json(encoded, "profile archive JSON");
  std::string payload;
  std::string checksum;
  try {
    yyjson_val* root = yyjson_doc_get_root(manifest_document);
    validate_keys(root, {"archiveKind", "schemaVersion", "checksumAlgorithm",
                         "payloadEncoding", "payloadChecksum", "payload"});
    yyjson_val* schema = required(root, "schemaVersion");
    if (read_string(root, "archiveKind") != kArchiveKind ||
        !yyjson_is_uint(schema) ||
        yyjson_get_uint(schema) != kProfileArchiveSchemaVersion ||
        read_string(root, "checksumAlgorithm") != kChecksumAlgorithm ||
        read_string(root, "payloadEncoding") != kPayloadEncoding) {
      throw std::runtime_error("Unsupported profile archive manifest");
    }
    checksum = read_string(root, "payloadChecksum", 64);
    if (!valid_digest(checksum)) {
      throw std::runtime_error("Profile archive checksum is malformed");
    }
    const auto decoded = base64_decode(read_string(root, "payload", kMaximumArchiveBytes),
                                       kMaximumPayloadBytes);
    payload.assign(reinterpret_cast<const char*>(decoded.data()), decoded.size());
    yyjson_doc_free(manifest_document);
  } catch (...) {
    yyjson_doc_free(manifest_document);
    throw;
  }
  if (digest_hex(payload) != checksum) {
    throw std::runtime_error("Profile archive payload checksum does not match");
  }

  yyjson_doc* payload_document = parse_json(payload, "profile archive payload");
  try {
    yyjson_val* root = yyjson_doc_get_root(payload_document);
    validate_keys(root, {"schemaVersion", "profile", "dailyAllowanceSeconds"});
    yyjson_val* schema = required(root, "schemaVersion");
    if (!yyjson_is_uint(schema) ||
        yyjson_get_uint(schema) != kProfileArchiveSchemaVersion) {
      throw std::runtime_error("Unsupported profile archive payload schema");
    }
    yyjson_val* value = required(root, "profile");
    validate_keys(value, {"id", "displayName", "role", "saveNamespace",
                          "contentPolicyRef", "timePolicyRef", "preferencesJson",
                          "backgroundRef", "avatar"});
    const std::string id = read_string(value, "id");
    const std::string display_name = read_string(value, "displayName");
    const auto role = parse_role(read_string(value, "role"));
    const std::string save_namespace = read_string(value, "saveNamespace");
    const auto content_policy = read_optional_string(value, "contentPolicyRef");
    const auto time_policy = read_optional_string(value, "timePolicyRef");
    const std::string preferences = read_string(value, "preferencesJson", 64 * 1024);
    std::string background_ref = "builtin:garden-morning";
    if (yyjson_val* background = yyjson_obj_get(value, "backgroundRef");
        background != nullptr) {
      if (!yyjson_is_str(background) || yyjson_get_len(background) == 0 ||
          yyjson_get_len(background) > 256) {
        throw std::runtime_error("Archive profile background reference is invalid");
      }
      background_ref = yyjson_get_str(background);
      if (!starts_with(background_ref, "builtin:")) {
        throw std::runtime_error("Archive profile background reference is invalid");
      }
    }
    validate_portable_text(id, "id");
    validate_portable_text(display_name, "displayName");
    validate_portable_text(save_namespace, "saveNamespace");

    yyjson_val* avatar = required(value, "avatar");
    const std::string avatar_kind = read_string(avatar, "kind");
    std::string avatar_ref;
    std::optional<std::vector<std::uint8_t>> portrait;
    std::optional<std::vector<std::uint8_t>> thumbnail;
    if (avatar_kind == "builtin") {
      validate_keys(avatar, {"kind", "reference"});
      avatar_ref = read_string(avatar, "reference");
      if (!starts_with(avatar_ref, "builtin:")) {
        throw std::runtime_error("Archive built-in avatar reference is invalid");
      }
    } else if (avatar_kind == "managedPng") {
      validate_keys(avatar, {"kind", "portrait", "thumbnail"});
      portrait = base64_decode(read_string(avatar, "portrait", 2 * 1024 * 1024),
                               kMaximumPortraitBytes);
      thumbnail = base64_decode(read_string(avatar, "thumbnail", 512 * 1024),
                                kMaximumThumbnailBytes);
      validate_managed_png(*portrait, 256, "Archive portrait");
      validate_managed_png(*thumbnail, 96, "Archive thumbnail");
      avatar_ref = "local:pending-restore";
    } else {
      throw std::runtime_error("Profile archive avatar kind is unsupported");
    }

    yyjson_val* allowance_value = required(root, "dailyAllowanceSeconds");
    std::optional<std::uint32_t> daily_allowance;
    if (!yyjson_is_null(allowance_value)) {
      if (!yyjson_is_uint(allowance_value) ||
          yyjson_get_uint(allowance_value) == 0 ||
          yyjson_get_uint(allowance_value) > 86'400) {
        throw std::runtime_error("Profile archive daily allowance is invalid");
      }
      daily_allowance =
          static_cast<std::uint32_t>(yyjson_get_uint(allowance_value));
    }
    if (role == ProfileRole::Child &&
        (!content_policy.has_value() || !time_policy.has_value() ||
         !daily_allowance.has_value())) {
      throw std::runtime_error(
          "Archived child profile requires content and daily time policies");
    }
    if (role == ProfileRole::Parent && daily_allowance.has_value()) {
      throw std::runtime_error("Archived parent profile cannot carry child allowance");
    }

    ParsedArchive result{
        .summary = ProfileArchiveSummary{
            .schema_version = kProfileArchiveSchemaVersion,
            .profile_id = id,
            .display_name = display_name,
            .role = role,
            .contains_personal_image = portrait.has_value(),
        },
        .profile = NewProfile{
            .id = id,
            .display_name = display_name,
            .role = role,
            .avatar_ref = avatar_ref,
            .save_namespace = save_namespace,
            .content_policy_ref = content_policy,
            .time_policy_ref = time_policy,
            .preferences_json = preferences,
            .background_ref = background_ref,
        },
        .daily_allowance_seconds = daily_allowance,
        .portrait = std::move(portrait),
        .thumbnail = std::move(thumbnail),
        .payload_checksum = checksum,
    };
    yyjson_doc_free(payload_document);
    return result;
  } catch (...) {
    yyjson_doc_free(payload_document);
    throw;
  }
}

void synchronize_file(FILE* file) {
  if (std::fflush(file) != 0) {
    throw std::runtime_error("Could not flush profile archive file");
  }
#ifdef _WIN32
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
  if (handle == INVALID_HANDLE_VALUE || FlushFileBuffers(handle) == 0) {
#else
  if (fsync(fileno(file)) != 0) {
#endif
    throw std::runtime_error("Could not synchronize profile archive file");
  }
}

void write_new_file(const std::filesystem::path& path,
                    const std::uint8_t* data, std::size_t size) {
  FILE* file = nullptr;
#ifdef _WIN32
  if (_wfopen_s(&file, path.c_str(), L"wbx") != 0) {
#else
  file = std::fopen(path.c_str(), "wbx");
  if (file == nullptr) {
#endif
    throw std::runtime_error("Could not create profile archive file");
  }
  try {
    if (std::fwrite(data, 1, size, file) != size) {
      throw std::runtime_error("Could not write profile archive file");
    }
    synchronize_file(file);
    if (std::fclose(file) != 0) {
      file = nullptr;
      throw std::runtime_error("Could not close profile archive file");
    }
    file = nullptr;
  } catch (...) {
    if (file != nullptr) {
      std::fclose(file);
    }
    throw;
  }
}

void write_new_file(const std::filesystem::path& path,
                    std::string_view contents) {
  write_new_file(path,
                 reinterpret_cast<const std::uint8_t*>(contents.data()),
                 contents.size());
}

void activate_new_file(const std::filesystem::path& pending,
                       const std::filesystem::path& destination) {
#ifdef _WIN32
  if (!MoveFileExW(pending.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error("Could not activate profile archive file");
  }
#else
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0U)
#endif
  if (syscall(SYS_renameat2, AT_FDCWD, pending.c_str(), AT_FDCWD,
              destination.c_str(), RENAME_NOREPLACE) != 0) {
    throw std::runtime_error(
        "Could not atomically activate new profile archive file");
  }
#endif
}

void write_archive(const std::filesystem::path& destination,
                   std::string_view contents) {
  if (destination.extension() != ".sprout-profile" ||
      destination.parent_path().empty()) {
    throw std::invalid_argument(
        "Profile export requires an explicit .sprout-profile destination");
  }
  std::error_code error;
  if (std::filesystem::symlink_status(destination.parent_path(), error).type() !=
          std::filesystem::file_type::directory ||
      std::filesystem::exists(destination, error)) {
    throw std::invalid_argument(
        "Profile archive parent must exist and destination must be new");
  }
  std::filesystem::path pending = destination;
  pending += ".pending";
  if (std::filesystem::exists(pending, error)) {
    throw std::invalid_argument("Profile archive pending path already exists");
  }
  try {
    write_new_file(pending, contents);
    activate_new_file(pending, destination);
  } catch (...) {
    std::filesystem::remove(pending, error);
    throw;
  }
}

std::string restored_asset_id(std::string_view profile_id,
                              std::string_view checksum) {
  static constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(profile_id.size() * 2 + 18);
  for (const unsigned char byte : profile_id) {
    result.push_back(hex[byte >> 4U]);
    result.push_back(hex[byte & 0x0fU]);
  }
  result += "-r";
  result.append(checksum.substr(0, 16));
  return result;
}

ManagedProfileImagePaths write_restored_image(
    const std::filesystem::path& root, const std::string& avatar_ref,
    const std::vector<std::uint8_t>& portrait,
    const std::vector<std::uint8_t>& thumbnail) {
  std::error_code error;
  const auto root_status = std::filesystem::symlink_status(root, error);
  if (error && root_status.type() != std::filesystem::file_type::not_found) {
    throw std::runtime_error("Could not inspect managed profile-image root");
  }
  if (root_status.type() == std::filesystem::file_type::not_found) {
    if (!std::filesystem::create_directories(root)) {
      throw std::runtime_error("Could not create managed profile-image root");
    }
  } else if (root_status.type() != std::filesystem::file_type::directory) {
    throw std::runtime_error("Managed profile-image root is not a directory");
  }
  const auto paths = resolve_managed_profile_image(root, avatar_ref);
  if (!std::filesystem::create_directory(paths.directory)) {
    throw std::runtime_error("Restored managed profile image already exists");
  }
  try {
    write_new_file(paths.portrait, portrait.data(), portrait.size());
    write_new_file(paths.thumbnail, thumbnail.data(), thumbnail.size());
    std::filesystem::permissions(
        paths.portrait,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(
        paths.thumbnail,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
    return paths;
  } catch (...) {
    std::filesystem::remove(paths.portrait, error);
    std::filesystem::remove(paths.thumbnail, error);
    std::filesystem::remove(paths.directory, error);
    throw;
  }
}

void remove_restored_image(const ManagedProfileImagePaths& paths) noexcept {
  std::error_code ignored;
  std::filesystem::remove(paths.portrait, ignored);
  std::filesystem::remove(paths.thumbnail, ignored);
  std::filesystem::remove(paths.directory, ignored);
}

}  // namespace

ProfileArchiveService::ProfileArchiveService(
    ProfileRepository& profiles, DailyTimePolicyStore& time_policy,
    std::filesystem::path managed_image_root)
    : profiles_(profiles),
      time_policy_(time_policy),
      managed_image_root_(std::move(managed_image_root)) {}

ProfileArchiveSummary ProfileArchiveService::export_profile(
    const std::string& profile_id, const std::filesystem::path& destination,
    ProfileArchiveExportOptions options) {
  const auto profile = profiles_.find_profile(profile_id);
  if (!profile.has_value() || profile->lifecycle != ProfileLifecycle::Active) {
    throw std::invalid_argument("Profile export requires an active profile");
  }
  std::optional<std::uint32_t> allowance;
  if (profile->role == ProfileRole::Child) {
    allowance = time_policy_.find_daily_allowance_seconds(profile_id);
    if (!allowance.has_value()) {
      throw std::runtime_error("Child profile has no concrete daily allowance");
    }
  }

  std::optional<std::vector<std::uint8_t>> portrait;
  std::optional<std::vector<std::uint8_t>> thumbnail;
  if (is_managed_profile_image_reference(profile->avatar_ref)) {
    if (!options.include_managed_avatar) {
      throw std::invalid_argument(
          "Managed profile image requires explicit export inclusion");
    }
    const auto paths =
        resolve_managed_profile_image(managed_image_root_, profile->avatar_ref);
    portrait = read_regular_file(paths.portrait, kMaximumPortraitBytes,
                                 "Managed profile portrait");
    thumbnail = read_regular_file(paths.thumbnail, kMaximumThumbnailBytes,
                                  "Managed profile thumbnail");
    validate_managed_png(*portrait, 256, "Managed profile portrait");
    validate_managed_png(*thumbnail, 96, "Managed profile thumbnail");
  } else if (!starts_with(profile->avatar_ref, "builtin:")) {
    throw std::runtime_error("Profile has an unsupported avatar reference");
  }

  const std::string payload =
      serialize_payload(*profile, allowance, portrait, thumbnail);
  const std::string checksum = digest_hex(payload);
  write_archive(destination, serialize_archive(payload, checksum));
  return ProfileArchiveSummary{
      .schema_version = kProfileArchiveSchemaVersion,
      .profile_id = profile->id,
      .display_name = profile->display_name,
      .role = profile->role,
      .contains_personal_image = portrait.has_value(),
  };
}

ProfileArchiveSummary ProfileArchiveService::inspect_archive(
    const std::filesystem::path& archive_path) const {
  return parse_archive(read_archive_file(archive_path)).summary;
}

ProfileArchiveSummary ProfileArchiveService::restore_profile(
    const std::filesystem::path& archive_path) {
  ParsedArchive archive = parse_archive(read_archive_file(archive_path));
  if (profiles_.find_profile(archive.profile.id).has_value()) {
    throw std::invalid_argument("Profile archive ID already exists");
  }
  for (const auto& existing : profiles_.list_profiles()) {
    if (existing.save_namespace == archive.profile.save_namespace) {
      throw std::invalid_argument("Profile archive save namespace already exists");
    }
  }
  if (time_policy_.find_daily_allowance_seconds(archive.profile.id).has_value()) {
    throw std::invalid_argument("Profile archive daily policy already exists");
  }

  std::optional<ManagedProfileImagePaths> restored_image;
  bool policy_created = false;
  try {
    if (archive.portrait.has_value() && archive.thumbnail.has_value()) {
      const std::string avatar_ref =
          "local:" + restored_asset_id(archive.profile.id,
                                        archive.payload_checksum);
      restored_image = write_restored_image(
          managed_image_root_, avatar_ref, *archive.portrait, *archive.thumbnail);
      archive.profile.avatar_ref = avatar_ref;
    }
    if (archive.daily_allowance_seconds.has_value()) {
      time_policy_.set_daily_allowance(archive.profile.id,
                                       *archive.daily_allowance_seconds);
      policy_created = true;
    }
    profiles_.create_profile(archive.profile);
    return archive.summary;
  } catch (...) {
    if (policy_created) {
      try {
        time_policy_.remove_unused_daily_allowance(archive.profile.id);
      } catch (...) {
      }
    }
    if (restored_image.has_value()) {
      remove_restored_image(*restored_image);
    }
    throw;
  }
}

}  // namespace sprout::launcher
