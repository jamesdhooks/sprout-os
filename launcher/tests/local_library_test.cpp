#include "sprout/launcher/local_library.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using sprout::launcher::LocalLibraryScanner;
using sprout::launcher::OnionSystem;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class TemporaryCard {
 public:
  TemporaryCard() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("sprout-library-" + std::to_string(nonce));
    std::filesystem::create_directories(root_ / "Roms/GB/Nested");
    std::filesystem::create_directories(root_ / "Roms/SFC/Imgs");
  }

  ~TemporaryCard() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }

  [[nodiscard]] const std::filesystem::path& root() const { return root_; }

  void add(std::string_view relative_path) {
    const auto path = root_ / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << "sanitized fixture";
  }

 private:
  std::filesystem::path root_;
};

void discovers_supported_items_deterministically() {
  TemporaryCard card;
  card.add("Roms/GB/Nested/Zelda.GB");
  card.add("Roms/GB/alpha game.zip");
  card.add("Roms/GB/readme.txt");
  card.add("Roms/SFC/Mario.sfc");
  card.add("Roms/SFC/Imgs/Mario.png");

  const LocalLibraryScanner scanner(card.root());
  const auto first = scanner.discover();
  const auto second = scanner.discover();
  require(first.warnings.empty(), "complete fixture card should not warn");
  require(first.items.size() == 3, "only supported ROM files should be discovered");
  require(first.items[0].title == "alpha game" &&
              first.items[1].title == "Mario" && first.items[2].title == "Zelda",
          "items should use deterministic case-insensitive title ordering");
  require(first.items[0].id == "onion:GB:alpha%20game.zip",
          "identity should encode the system-relative path");
  require(first.items[1].system == OnionSystem::SuperNintendo,
          "SFC files should retain their typed system");
  require(first.items[2].id == "onion:GB:Nested%2FZelda.GB",
          "nested path should remain part of stable identity");
  require(first.items[0].id == second.items[0].id &&
              first.items[1].id == second.items[1].id &&
              first.items[2].id == second.items[2].id,
          "repeated scans should preserve identities and order");
}

void missing_roots_degrade_with_warnings() {
  TemporaryCard card;
  std::filesystem::remove_all(card.root() / "Roms/SFC");
  const auto result = LocalLibraryScanner(card.root()).discover();
  require(result.items.empty(), "missing system root should not fabricate items");
  require(result.warnings.size() == 1 &&
              result.warnings[0] == "SFC ROM directory is unavailable",
          "missing system root should produce a recoverable system warning");

  const auto unavailable =
      LocalLibraryScanner(card.root() / "missing-card").discover();
  require(unavailable.items.empty() && unavailable.warnings.size() == 1,
          "missing card should fail without throwing");
}

#ifndef _WIN32
void linked_rom_cannot_escape_system_root() {
  TemporaryCard card;
  const auto outside = card.root().parent_path() /
                       (card.root().filename().string() + "-outside.gb");
  {
    std::ofstream output(outside, std::ios::binary);
    output << "outside fixture";
  }
  std::filesystem::create_symlink(outside,
                                  card.root() / "Roms/GB/linked.gb");
  const auto result = LocalLibraryScanner(card.root()).discover();
  require(result.items.empty(),
          "a linked ROM outside the canonical system root must be ignored");
  std::filesystem::remove(outside);
}
#endif

}  // namespace

int main() {
  try {
    discovers_supported_items_deterministically();
    missing_roots_degrade_with_warnings();
#ifndef _WIN32
    linked_rom_cannot_escape_system_root();
#endif
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
