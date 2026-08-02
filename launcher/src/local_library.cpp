#include "sprout/launcher/local_library.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace sprout::launcher {
namespace {

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
      return static_cast<char>(byte + ('a' - 'A'));
    }
    return static_cast<char>(byte);
  });
  return value;
}

bool within(const std::filesystem::path& path,
            const std::filesystem::path& root) {
  auto path_part = path.begin();
  for (auto root_part = root.begin(); root_part != root.end();
       ++root_part, ++path_part) {
    if (path_part == path.end() || *path_part != *root_part) {
      return false;
    }
  }
  return true;
}

bool supported_extension(const OnionSystemContract& contract,
                         const std::filesystem::path& path) {
  const auto extension = lowercase(path.extension().string());
  return std::find(contract.extensions.begin(), contract.extensions.end(),
                   extension) != contract.extensions.end();
}

std::string encode_identity_part(std::string_view value) {
  std::ostringstream encoded;
  encoded << std::uppercase << std::hex;
  for (const unsigned char byte : value) {
    const bool ascii_alphanumeric =
        (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        (byte >= '0' && byte <= '9');
    if (ascii_alphanumeric || byte == '-' || byte == '_' || byte == '.') {
      encoded << static_cast<char>(byte);
    } else {
      encoded << '%' << std::setw(2) << std::setfill('0')
              << static_cast<unsigned int>(byte);
    }
  }
  return encoded.str();
}

void add_warning_once(std::vector<std::string>& warnings, std::string warning) {
  if (std::find(warnings.begin(), warnings.end(), warning) == warnings.end()) {
    warnings.push_back(std::move(warning));
  }
}

}  // namespace

LocalLibraryScanner::LocalLibraryScanner(std::filesystem::path sd_card_root)
    : sd_card_root_(std::move(sd_card_root)) {}

LibraryScanResult LocalLibraryScanner::discover() const {
  LibraryScanResult result;
  std::error_code error;
  const auto sd_card_root =
      std::filesystem::weakly_canonical(sd_card_root_, error);
  if (error || !std::filesystem::is_directory(sd_card_root, error) || error) {
    result.warnings.push_back("The configured Onion SD-card root is unavailable");
    return result;
  }

  for (const auto system :
       std::array{OnionSystem::GameBoy, OnionSystem::SuperNintendo}) {
    const auto contract = onion_system_contract(system);
    if (!contract.has_value()) {
      continue;
    }
    error.clear();
    const auto system_root = std::filesystem::weakly_canonical(
        sd_card_root / contract->rom_directory, error);
    if (error || !within(system_root, sd_card_root) ||
        !std::filesystem::is_directory(system_root, error) || error) {
      result.warnings.push_back(std::string(contract->id) +
                                " ROM directory is unavailable");
      continue;
    }

    std::filesystem::recursive_directory_iterator iterator(
        system_root, std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
      result.warnings.push_back(std::string(contract->id) +
                                " ROM directory could not be scanned");
      continue;
    }
    while (iterator != end) {
      if (error) {
        add_warning_once(result.warnings,
                         std::string(contract->id) +
                             " ROM directory could not be scanned completely");
        error.clear();
        iterator.increment(error);
        continue;
      }

      const auto entry_path = iterator->path();
      if (iterator->is_regular_file(error) && !error &&
          supported_extension(*contract, entry_path)) {
        const auto canonical_path =
            std::filesystem::weakly_canonical(entry_path, error);
        if (!error && within(canonical_path, system_root)) {
          const auto relative_path =
              std::filesystem::relative(canonical_path, system_root, error);
          if (!error && !relative_path.empty()) {
            result.items.push_back(EmulatedLibraryItem{
                .schema_version = EmulatedLibraryItem::kSchemaVersion,
                .id = "onion:" + std::string(contract->id) + ":" +
                      encode_identity_part(path_as_utf8(relative_path)),
                .title = path_as_utf8(canonical_path.stem()),
                .system = system,
                .rom_path = canonical_path,
            });
          }
        }
      }
      if (error) {
        add_warning_once(result.warnings,
                         std::string(contract->id) +
                             " ROM directory could not be scanned completely");
        error.clear();
      }
      iterator.increment(error);
    }
  }

  std::sort(result.items.begin(), result.items.end(),
            [](const EmulatedLibraryItem& left,
               const EmulatedLibraryItem& right) {
              const auto left_title = lowercase(left.title);
              const auto right_title = lowercase(right.title);
              if (left_title != right_title) {
                return left_title < right_title;
              }
              return left.id < right.id;
            });
  return result;
}

}  // namespace sprout::launcher
