#include "sprout/launcher/local_library.hpp"

#include <algorithm>
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
  card.add("Roms/GB/Imgs/alpha game.png");
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
  require(first.items[0].artwork_path ==
              std::filesystem::weakly_canonical(
                  card.root() / "Roms/GB/Imgs/alpha game.png") &&
              first.items[1].artwork_path ==
                  std::filesystem::weakly_canonical(
                      card.root() / "Roms/SFC/Imgs/Mario.png"),
          "Onion Imgs artwork should follow the ROM filename on every system");
  require(first.items[2].id == "onion:GB:Nested%2FZelda.GB",
          "nested path should remain part of stable identity");
  require(first.items[0].id == second.items[0].id &&
              first.items[1].id == second.items[1].id &&
              first.items[2].id == second.items[2].id,
          "repeated scans should preserve identities and order");
}

void discovers_the_extended_onion_catalogue() {
  TemporaryCard card;
  card.add("Roms/FC/Family Adventure.nes");
  card.add("Roms/GBC/Color Quest.gbc");
  card.add("Roms/GBA/Advance Journey.gba");
  card.add("Roms/MD/Speed Trail.md");
  card.add("Roms/MS/Classic.sms");
  card.add("Roms/GG/Pocket.gg");
  card.add("Roms/SEGACD/Disc Adventure.cue");
  card.add("Roms/PCE/Bonk.pce");
  card.add("Roms/NEOGEO/Fighter.zip");
  card.add("Roms/ARCADE/Cabinet.zip");
  card.add("Roms/PS/Memory Disc.chd");
  card.add("Roms/PS/_hidden/multi-disc/Memory Disc (Disc 2).chd");
  card.add("Roms/PICO/Cart.p8");
  card.add("Roms/PICO/Artwork.png");
  card.add("Roms/PICO/Imgs/Cart.png");
  card.add("Roms/PS/cover.png");

  const auto result = LocalLibraryScanner(card.root()).discover();
  require(result.items.size() == 13,
          "every registered Tiny Best Set system should discover valid files");
  require(result.warnings.empty(),
          "missing unrelated systems must not prevent discovered systems");
  const auto contains = [&](std::string_view id) {
    return std::any_of(result.items.begin(), result.items.end(),
                       [&](const auto& item) { return item.id == id; });
  };
  require(contains("onion:NES:Family%20Adventure.nes"),
          "NES should map Onion FC content to the household seed label");
  require(contains("onion:PS:Memory%20Disc.chd"),
          "PlayStation CHD files should be discovered");
  require(!contains("onion:PS:_hidden%2Fmulti-disc%2FMemory%20Disc%20%28Disc%202%29.chd"),
          "hidden multi-disc dependencies must not become duplicate games");
  require(contains("onion:PICO:Cart.p8") &&
              contains("onion:PICO:Artwork.png"),
          "PICO-8 carts should include native and PNG cartridge forms");
  const auto cart = std::find_if(result.items.begin(), result.items.end(),
                                 [](const auto& item) {
                                   return item.id == "onion:PICO:Cart.p8";
                                 });
  require(cart != result.items.end() && !cart->artwork_path.empty() &&
              !contains("onion:PICO:Imgs%2FCart.png"),
          "PICO artwork folders must provide covers without becoming games");
}

void shared_catalogue_preserves_arcade_shortnames_and_friendly_titles() {
  TemporaryCard card;
  card.add("Roms/ARCADE/galaga.zip");
  card.add("Roms/ARCADE/Imgs/galaga.png");
  card.add("Sprout/catalogue/games.json");
  {
    std::ofstream catalogue(card.root() / "Sprout/catalogue/games.json",
                            std::ios::binary | std::ios::trunc);
    catalogue << R"json({"entries":[{"platform":"ARCADE","cart":"galaga.zip","id":"onion:ARCADE:galaga.zip","title":"Galaga (Namco rev. B)","cover":"Roms/ARCADE/Imgs/galaga.png"}]})json";
  }

  const auto result = LocalLibraryScanner(card.root()).discover();
  require(result.items.size() == 1,
          "catalogue metadata must not create extra library items");
  require(result.items.front().title == "Galaga (Namco rev. B)" &&
              result.items.front().id == "onion:ARCADE:galaga.zip" &&
              !result.items.front().artwork_path.empty(),
          "shared catalogue should map short ROM names to seed-facing metadata");
}

void missing_roots_degrade_with_warnings() {
  TemporaryCard card;
  std::filesystem::remove_all(card.root() / "Roms/SFC");
  const auto result = LocalLibraryScanner(card.root()).discover();
  require(result.items.empty(), "missing system root should not fabricate items");
  require(result.warnings.empty(),
          "uninstalled optional Onion systems should not produce noisy warnings");

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
    discovers_the_extended_onion_catalogue();
    shared_catalogue_preserves_arcade_shortnames_and_friendly_titles();
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
