#include "sprout/launcher/local_library.hpp"
#include "sprout/launcher/pico8_catalogue.hpp"

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

std::filesystem::path find_onion_artwork(
    const std::filesystem::path& system_root,
    const std::filesystem::path& rom_relative_path) {
  constexpr std::array<std::string_view, 3> kArtworkExtensions{
      ".png", ".jpg", ".jpeg"};
  const std::array<std::filesystem::path, 2> artwork_bases{
      system_root / "Imgs" / rom_relative_path,
      system_root / "Imgs" / rom_relative_path.filename(),
  };
  // Household payloads generate compact, aspect-preserving card art here.
  // Prefer it over Onion's full-resolution source cover whenever present.
  for (auto base : artwork_bases) {
    auto thumbnail = system_root / "Imgs" / ".sprout-thumbs" /
        base.lexically_relative(system_root / "Imgs");
    thumbnail.replace_extension(".png");
    std::error_code error;
    if (std::filesystem::is_regular_file(thumbnail, error) && !error) {
      const auto canonical = std::filesystem::weakly_canonical(thumbnail, error);
      if (!error && within(canonical, system_root)) return canonical;
    }
  }
  for (auto base : artwork_bases) {
    for (const auto extension : kArtworkExtensions) {
      auto candidate = base;
      candidate.replace_extension(extension);
      std::error_code error;
      if (!std::filesystem::is_regular_file(candidate, error) || error) {
        continue;
      }
      const auto canonical = std::filesystem::weakly_canonical(candidate, error);
      if (!error && within(canonical, system_root)) return canonical;
    }
  }
  return {};
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

  for (const auto system : onion_supported_systems()) {
    const auto contract = onion_system_contract(system);
    if (!contract.has_value()) {
      continue;
    }
    error.clear();
    const auto configured_system_root = sd_card_root / contract->rom_directory;
    if (!std::filesystem::exists(configured_system_root, error) && !error) {
      continue;
    }
    if (error) {
      result.warnings.push_back(std::string(contract->id) +
                                " ROM directory is unavailable");
      continue;
    }
    const auto system_root = std::filesystem::weakly_canonical(
        configured_system_root, error);
    if (error || !within(system_root, sd_card_root)) {
      result.warnings.push_back(std::string(contract->id) +
                                " ROM directory is unavailable");
      continue;
    }
    error.clear();
    if (!std::filesystem::is_directory(system_root, error) && !error) {
      continue;
    }
    if (error) {
      result.warnings.push_back(std::string(contract->id) +
                                " ROM directory is unavailable");
      continue;
    }

    Pico8Catalogue pico8_catalogue(sd_card_root);
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
      if (iterator->is_directory(error) && !error &&
          lowercase(entry_path.filename().string()) == "imgs") {
        iterator.disable_recursion_pending();
        iterator.increment(error);
        continue;
      }
      // Onion uses _hidden for multi-disc payloads referenced by a visible
      // M3U. Those CHDs are dependencies, not separate library entries.
      if (iterator->is_directory(error) && !error &&
          lowercase(entry_path.filename().string()) == "_hidden") {
        iterator.disable_recursion_pending();
        iterator.increment(error);
        continue;
      }
      // Profile-cart copies are launch material owned by Sprout, not library
      // items. Skipping the directory also prevents recursive duplicate scans.
      if (contract->id == "PICO" && iterator->is_directory(error) &&
          entry_path.filename() == ".sprout-profiles") {
        iterator.disable_recursion_pending();
        iterator.increment(error);
        continue;
      }
      if (iterator->is_regular_file(error) && !error &&
          supported_extension(*contract, entry_path)) {
        const auto canonical_path =
            std::filesystem::weakly_canonical(entry_path, error);
        if (!error && within(canonical_path, system_root)) {
          const auto relative_path =
              std::filesystem::relative(canonical_path, system_root, error);
          if (!error && !relative_path.empty()) {
            auto item_id = "onion:" + std::string(contract->id) + ":" +
                           encode_identity_part(path_as_utf8(relative_path));
            auto title = path_as_utf8(canonical_path.stem());
            std::filesystem::path artwork_path;
            if (const auto known =
                    pico8_catalogue.find(contract->id, relative_path)) {
              item_id = known->id;
              title = known->title;
              artwork_path = known->artwork_path;
            }
            if (artwork_path.empty()) {
              artwork_path = find_onion_artwork(system_root, relative_path);
            }
            result.items.push_back(EmulatedLibraryItem{
                .schema_version = EmulatedLibraryItem::kSchemaVersion,
                .id = std::move(item_id),
                .title = std::move(title),
                .system = system,
                .rom_path = canonical_path,
                .artwork_path = std::move(artwork_path),
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
