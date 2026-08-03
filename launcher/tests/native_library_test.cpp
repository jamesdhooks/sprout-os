#include "sprout/launcher/native_library.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

class TemporaryPackages {
 public:
  TemporaryPackages() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("sprout-native-library-" + std::to_string(nonce));
    std::filesystem::create_directories(root_);
  }

  ~TemporaryPackages() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }

  const std::filesystem::path& root() const { return root_; }

  void add(std::string directory, std::string id, std::string title,
           std::string audience) {
    const auto package = root_ / directory;
    std::filesystem::create_directories(package);
    std::ofstream manifest(package / "manifest.json");
    manifest << "{\n"
             << "  \"schemaVersion\": 1,\n"
             << "  \"id\": \"" << id << "\",\n"
             << "  \"title\": \"" << title << "\",\n"
             << "  \"version\": \"1.0.0\",\n"
             << "  \"runtimeVersion\": 1,\n"
             << "  \"entrypoint\": \"game.lua\",\n"
             << "  \"logicalResolution\": [320, 240],\n"
             << "  \"audience\": \"" << audience << "\",\n"
             << "  \"capabilities\": []\n"
             << "}\n";
    std::ofstream game(package / "game.lua");
    game << "function init() end\nfunction update(a) end\n"
            "function render() end\nfunction snapshot() return '' end\n";
  }

 private:
  std::filesystem::path root_;
};

void discovers_valid_packages_and_audience() {
  TemporaryPackages packages;
  packages.add("snake", "sprout.snake", "Sprout Snake", "family");
  packages.add("tools", "sprout.parent-tools", "Parent Tools", "parent");
  std::filesystem::create_directories(packages.root() / "broken");
  std::ofstream(packages.root() / "broken" / "manifest.json") << "{}";

  const auto result =
      sprout::launcher::NativePackageScanner(packages.root()).discover();
  require(result.items.size() == 2, "two valid packages should be discovered");
  require(result.items[0].id == "arcade:sprout.parent-tools" &&
              !result.items[0].child_visible &&
              result.items[1].id == "arcade:sprout.snake" &&
              result.items[1].child_visible,
          "package identity, ordering, and audience should be preserved");
  require(result.warnings.size() == 1,
          "invalid package should degrade to one warning");
}

void duplicate_and_missing_roots_fail_closed() {
  TemporaryPackages packages;
  packages.add("one", "sprout.duplicate", "First", "family");
  packages.add("two", "sprout.duplicate", "Second", "family");
  const auto duplicated =
      sprout::launcher::NativePackageScanner(packages.root()).discover();
  require(duplicated.items.empty() && !duplicated.warnings.empty(),
          "duplicate identities should all be unavailable");

  const auto missing = sprout::launcher::NativePackageScanner(
                           packages.root() / "missing")
                           .discover();
  require(missing.items.empty() && missing.warnings.size() == 1,
          "missing package root should return a warning without throwing");
}

}  // namespace

int main() {
  try {
    discovers_valid_packages_and_audience();
    duplicate_and_missing_roots_fail_closed();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
