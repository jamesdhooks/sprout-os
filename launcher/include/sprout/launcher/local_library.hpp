#pragma once

#include "sprout/launcher/onion_launch_adapter.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sprout::launcher {

struct EmulatedLibraryItem {
  static constexpr int kSchemaVersion = 1;

  int schema_version{kSchemaVersion};
  std::string id;
  std::string title;
  OnionSystem system;
  std::filesystem::path rom_path;
};

struct LibraryScanResult {
  std::vector<EmulatedLibraryItem> items;
  std::vector<std::string> warnings;
};

class LocalLibraryScanner {
 public:
  explicit LocalLibraryScanner(std::filesystem::path sd_card_root);

  [[nodiscard]] LibraryScanResult discover() const;

 private:
  std::filesystem::path sd_card_root_;
};

}  // namespace sprout::launcher
