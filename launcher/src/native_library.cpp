#include "sprout/launcher/native_library.hpp"

#include "sprout/runtime/package.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   if (character >= 'A' && character <= 'Z') {
                     return static_cast<char>(character + ('a' - 'A'));
                   }
                   return static_cast<char>(character);
                 });
  return value;
}

}  // namespace

NativePackageScanner::NativePackageScanner(std::filesystem::path packages_root)
    : packages_root_(std::move(packages_root)) {}

NativeLibraryScanResult NativePackageScanner::discover() const {
  NativeLibraryScanResult result;
  std::error_code error;
  const auto packages_root =
      std::filesystem::canonical(packages_root_, error);
  if (error || !std::filesystem::is_directory(packages_root, error) || error) {
    result.warnings.push_back("The configured Sprout Arcade package root is unavailable");
    return result;
  }

  std::filesystem::directory_iterator iterator(
      packages_root, std::filesystem::directory_options::skip_permission_denied,
      error);
  const std::filesystem::directory_iterator end;
  if (error) {
    result.warnings.push_back("The Sprout Arcade package root could not be scanned");
    return result;
  }
  while (iterator != end) {
    if (error) {
      result.warnings.push_back(
          "The Sprout Arcade package root could not be scanned completely");
      error.clear();
      iterator.increment(error);
      continue;
    }
    if (iterator->is_directory(error) && !error) {
      try {
        const auto package = sprout::runtime::load_package(iterator->path());
        result.items.push_back(NativeLibraryItem{
            .schema_version = NativeLibraryItem::kSchemaVersion,
            .id = "arcade:" + package.id,
            .title = package.title,
            .package_version = package.version,
            .package_root = package.root,
            .child_visible =
                package.audience == sprout::runtime::PackageAudience::Family,
        });
      } catch (const std::exception& package_error) {
        result.warnings.push_back(
            "Arcade package " + path_as_utf8(iterator->path().filename()) +
            " is unavailable: " + package_error.what());
      }
    }
    error.clear();
    iterator.increment(error);
  }

  std::map<std::string, std::size_t> identities;
  for (const auto& item : result.items) {
    ++identities[item.id];
  }
  std::set<std::string> duplicates;
  for (const auto& [identity, count] : identities) {
    if (count > 1) {
      duplicates.insert(identity);
      result.warnings.push_back(
          "Arcade package identity is duplicated and unavailable: " + identity);
    }
  }
  result.items.erase(
      std::remove_if(result.items.begin(), result.items.end(),
                     [&](const NativeLibraryItem& item) {
                       return duplicates.contains(item.id);
                     }),
      result.items.end());
  std::sort(result.items.begin(), result.items.end(),
            [](const NativeLibraryItem& left, const NativeLibraryItem& right) {
              const auto left_title = lowercase(left.title);
              const auto right_title = lowercase(right.title);
              return left_title == right_title ? left.id < right.id
                                               : left_title < right_title;
            });
  return result;
}

}  // namespace sprout::launcher
