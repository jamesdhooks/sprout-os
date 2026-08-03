#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sprout::launcher {

struct NativeLibraryItem {
  static constexpr int kSchemaVersion = 1;

  int schema_version{kSchemaVersion};
  std::string id;
  std::string title;
  std::string package_version;
  std::filesystem::path package_root;
  std::filesystem::path artwork_path;
  bool child_visible{false};
};

struct NativeLibraryScanResult {
  std::vector<NativeLibraryItem> items;
  std::vector<std::string> warnings;
};

class NativePackageScanner {
 public:
  explicit NativePackageScanner(std::filesystem::path packages_root);

  [[nodiscard]] NativeLibraryScanResult discover() const;

 private:
  std::filesystem::path packages_root_;
};

}  // namespace sprout::launcher
