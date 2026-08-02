#include "sprout/launcher/library_presentation.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sprout::launcher {

std::vector<LibraryEntry> make_demo_library() {
  const auto root = std::filesystem::temp_directory_path() /
                    "sprout-sanitized-preview-library" / "Roms";
  const auto entry = [&](std::string id, std::string title, OnionSystem system,
                         std::string_view relative_path, bool favorite,
                         std::optional<std::size_t> recent_rank, bool allowed,
                         std::string unavailable_reason = {}) {
    return LibraryEntry{
        .item = EmulatedLibraryItem{
            .schema_version = EmulatedLibraryItem::kSchemaVersion,
            .id = std::move(id),
            .title = std::move(title),
            .system = system,
            .rom_path = root / relative_path,
        },
        .favorite = favorite,
        .recent_rank = recent_rank,
        .launch_allowed = allowed,
        .unavailable_reason = std::move(unavailable_reason),
    };
  };
  return {
      entry("preview:star-trail", "Star Trail", OnionSystem::SuperNintendo,
            "SFC/Star Trail.sfc", true, 1, true),
      entry("preview:meadow-quest", "Meadow Quest", OnionSystem::GameBoy,
            "GB/Meadow Quest.gb", true, 2, true),
      entry("preview:puzzle-garden", "Puzzle Garden", OnionSystem::GameBoy,
            "GB/Puzzle Garden.gb", false, 3, true),
      entry("preview:family-kart", "Family Kart", OnionSystem::SuperNintendo,
            "SFC/Family Kart.sfc", true, std::nullopt, true),
      entry("preview:moon-harbor", "Moon Harbor", OnionSystem::GameBoy,
            "GB/Moon Harbor.gb", false, std::nullopt, false,
            "ASK A PARENT TO ALLOW THIS GAME"),
  };
}

}  // namespace sprout::launcher
