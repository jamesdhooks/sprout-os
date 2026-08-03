#include "sprout/runtime/package.hpp"

#include <yyjson.h>

#include <fstream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string_view>

namespace sprout::runtime {
namespace {

constexpr std::uint32_t kPackageSchemaVersion = 1;
constexpr std::uint32_t kRuntimeVersion = 1;
constexpr std::uintmax_t kMaximumManifestBytes = 64U * 1024U;

void validate_keys(yyjson_val* object,
                   std::initializer_list<std::string_view> allowed) {
  if (!yyjson_is_obj(object)) {
    throw std::runtime_error("Package manifest should be an object");
  }
  const std::set<std::string_view> allowed_keys(allowed);
  std::set<std::string> observed;
  yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string name = yyjson_get_str(key);
    if (!allowed_keys.contains(name) || !observed.insert(name).second) {
      throw std::runtime_error(
          "Package manifest contains an unknown or duplicate field: " + name);
    }
  }
}

yyjson_val* required(yyjson_val* object, const char* key) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (value == nullptr) {
    throw std::runtime_error(std::string("Package manifest is missing field: ") + key);
  }
  return value;
}

std::string read_text(yyjson_val* object, const char* key) {
  yyjson_val* value = required(object, key);
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0) {
    throw std::runtime_error(std::string("Package field should be text: ") + key);
  }
  std::string result(yyjson_get_str(value), yyjson_get_len(value));
  if (result.find('\0') != std::string::npos) {
    throw std::runtime_error(std::string("Package field contains a null byte: ") + key);
  }
  return result;
}

std::uint32_t read_version(yyjson_val* object, const char* key) {
  yyjson_val* value = required(object, key);
  if (!yyjson_is_uint(value) || yyjson_get_uint(value) > UINT32_MAX) {
    throw std::runtime_error(std::string("Package field should be a version number: ") +
                             key);
  }
  return static_cast<std::uint32_t>(yyjson_get_uint(value));
}

std::string read_file(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > kMaximumManifestBytes) {
    throw std::runtime_error("Package manifest is unavailable or too large");
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Could not open package manifest");
  }
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

bool is_within(const std::filesystem::path& root,
               const std::filesystem::path& candidate) {
  auto root_part = root.begin();
  auto candidate_part = candidate.begin();
  while (root_part != root.end() && candidate_part != candidate.end()) {
    if (*root_part != *candidate_part) {
      return false;
    }
    ++root_part;
    ++candidate_part;
  }
  return root_part == root.end();
}

}  // namespace

PackageManifest load_package(const std::filesystem::path& package_root) {
  std::error_code path_error;
  const auto root = std::filesystem::canonical(package_root, path_error);
  if (path_error || !std::filesystem::is_directory(root)) {
    throw std::runtime_error("Package root is not a readable directory");
  }

  const std::string encoded = read_file(root / "manifest.json");
  yyjson_read_err read_error{};
  yyjson_doc* document = yyjson_read_opts(
      const_cast<char*>(encoded.data()), encoded.size(), YYJSON_READ_NOFLAG,
      nullptr, &read_error);
  if (document == nullptr) {
    throw std::runtime_error(std::string("Invalid package manifest JSON: ") +
                             read_error.msg);
  }

  try {
    yyjson_val* manifest = yyjson_doc_get_root(document);
    validate_keys(manifest,
                  {"schemaVersion", "id", "title", "version", "runtimeVersion",
                   "entrypoint", "logicalResolution", "capabilities"});

    PackageManifest package{
        .schema_version = read_version(manifest, "schemaVersion"),
        .id = read_text(manifest, "id"),
        .title = read_text(manifest, "title"),
        .version = read_text(manifest, "version"),
        .runtime_version = read_version(manifest, "runtimeVersion"),
        .root = root,
    };
    if (package.schema_version != kPackageSchemaVersion) {
      throw std::runtime_error("Unsupported package manifest schema version");
    }
    if (package.runtime_version != kRuntimeVersion) {
      throw std::runtime_error("Package requires an unsupported runtime version");
    }
    if (!std::regex_match(package.id,
                          std::regex("[a-z0-9]+(?:[.-][a-z0-9]+)+"))) {
      throw std::runtime_error("Package id is invalid");
    }
    if (!std::regex_match(package.version,
                          std::regex("[0-9]+\\.[0-9]+\\.[0-9]+"))) {
      throw std::runtime_error("Package version should use MAJOR.MINOR.PATCH");
    }

    const std::filesystem::path entrypoint = read_text(manifest, "entrypoint");
    if (entrypoint.is_absolute() || entrypoint.extension() != ".lua") {
      throw std::runtime_error("Package entrypoint should be a relative Lua file");
    }
    package.entrypoint = std::filesystem::canonical(root / entrypoint, path_error);
    if (path_error || !std::filesystem::is_regular_file(package.entrypoint) ||
        !is_within(root, package.entrypoint)) {
      throw std::runtime_error("Package entrypoint escapes or is missing from its root");
    }

    yyjson_val* resolution = required(manifest, "logicalResolution");
    if (!yyjson_is_arr(resolution) || yyjson_arr_size(resolution) != 2) {
      throw std::runtime_error("Package logicalResolution should contain width and height");
    }
    yyjson_val* width = yyjson_arr_get_first(resolution);
    yyjson_val* height = yyjson_arr_get(resolution, 1);
    if (!yyjson_is_uint(width) || !yyjson_is_uint(height) ||
        yyjson_get_uint(width) < 64 || yyjson_get_uint(width) > 640 ||
        yyjson_get_uint(height) < 64 || yyjson_get_uint(height) > 480) {
      throw std::runtime_error("Package logical resolution is outside supported bounds");
    }
    package.logical_width = static_cast<int>(yyjson_get_uint(width));
    package.logical_height = static_cast<int>(yyjson_get_uint(height));

    yyjson_val* capabilities = required(manifest, "capabilities");
    if (!yyjson_is_arr(capabilities)) {
      throw std::runtime_error("Package capabilities should be an array");
    }
    const std::set<std::string_view> supported{"events", "local-storage"};
    std::set<std::string> observed;
    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val* capability = nullptr;
    yyjson_arr_foreach(capabilities, index, maximum, capability) {
      if (!yyjson_is_str(capability)) {
        throw std::runtime_error("Package capability should be text");
      }
      const std::string name = yyjson_get_str(capability);
      if (!supported.contains(name) || !observed.insert(name).second) {
        throw std::runtime_error("Package capability is unsupported or duplicated: " +
                                 name);
      }
      package.capabilities.push_back(name);
    }

    yyjson_doc_free(document);
    return package;
  } catch (...) {
    yyjson_doc_free(document);
    throw;
  }
}

}  // namespace sprout::runtime
