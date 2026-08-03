#include "sprout/launcher/library_presentation.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using sprout::launcher::Action;
using sprout::launcher::EmulatedLibraryItem;
using sprout::launcher::LibraryEntry;
using sprout::launcher::LibraryPresentation;
using sprout::launcher::LibraryPresentationEventType;
using sprout::launcher::LibrarySection;
using sprout::launcher::OnionSystem;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

LibraryEntry entry(std::string id, std::string title, OnionSystem system,
                   bool favorite, std::optional<std::size_t> recent_rank,
                   bool allowed = true) {
  return LibraryEntry{
      .item = EmulatedLibraryItem{
          .schema_version = EmulatedLibraryItem::kSchemaVersion,
          .id = std::move(id),
          .title = std::move(title),
          .system = system,
          .rom_path = std::filesystem::temp_directory_path() / "fixture.rom",
      },
      .favorite = favorite,
      .recent_rank = recent_rank,
      .launch_allowed = allowed,
      .unavailable_reason = allowed ? "" : "Ask a parent to allow this game",
  };
}

std::vector<LibraryEntry> entries() {
  return {
      entry("game-a", "Alpha", OnionSystem::GameBoy, false, 2),
      entry("game-b", "Bravo", OnionSystem::SuperNintendo, true, 1),
      entry("game-c", "Charlie", OnionSystem::GameBoy, true, std::nullopt,
            false),
  };
}

void menu_targets_map_to_sections() {
  using sprout::launcher::library_section_for_menu_target;
  require(library_section_for_menu_target("Continue") == LibrarySection::Recent,
          "Continue should open recent games");
  require(library_section_for_menu_target("Favorites") == LibrarySection::Favorites,
          "Favorites should open favorite games");
  require(library_section_for_menu_target("See All") == LibrarySection::All &&
              library_section_for_menu_target("All Games") == LibrarySection::All,
          "child and parent all-game labels should share the all section");
  require(!library_section_for_menu_target("Onion Tools").has_value(),
          "unrelated menu targets must not open the library");
}

void demo_library_is_sanitized_and_useful() {
  const auto demo = sprout::launcher::make_demo_library();
  require(demo.size() == 5, "demo library should remain small and deterministic");
  require(sprout::launcher::starts_with(demo[0].item.id, "preview:") &&
              demo[0].item.rom_path.is_absolute(),
          "demo entries should be clearly synthetic typed targets");
}

void filters_and_recent_order_are_deterministic() {
  LibraryPresentation recent(entries(), LibrarySection::Recent);
  require(recent.entries().size() == 2,
          "recent section should contain ranked entries only");
  require(recent.entries()[0].item.id == "game-b" &&
              recent.entries()[1].item.id == "game-a",
          "recent section should sort by explicit rank");

  LibraryPresentation favorites(entries(), LibrarySection::Favorites);
  require(favorites.entries().size() == 2,
          "favorites section should contain favorite entries only");
  require(favorites.entries()[0].item.id == "game-b" &&
              favorites.entries()[1].item.id == "game-c",
          "favorites should preserve catalogue order");

  LibraryPresentation all(entries(), LibrarySection::All);
  require(all.entries().size() == 3, "all section should retain every entry");
}

void navigation_launch_and_unavailable_are_explicit() {
  LibraryPresentation library(entries(), LibrarySection::All);
  (void)library.handle(Action::Up);
  require(library.focus_index() == 2, "up should wrap to the final item");
  auto event = library.handle(Action::Confirm);
  require(event.has_value() &&
              event->type == LibraryPresentationEventType::Unavailable &&
              !event->launch_target.has_value(),
          "blocked item should emit a recoverable unavailable event");
  require(event->message == "Ask a parent to allow this game",
          "unavailable event should retain its family-facing reason");

  (void)library.handle(Action::Down);
  event = library.handle(Action::Confirm);
  require(event.has_value() &&
              event->type == LibraryPresentationEventType::LaunchRequested &&
              event->launch_target.has_value(),
          "allowed item should emit a typed launch request");
  require(event->launch_target->item_id == "game-a" &&
              event->launch_target->system == OnionSystem::GameBoy &&
              event->launch_target->launch_allowed,
          "launch request should retain identity, system, and permission");

  event = library.handle(Action::Back);
  require(event.has_value() &&
              event->type == LibraryPresentationEventType::BackRequested,
          "back should return to the launcher home");
}

void empty_section_is_safe() {
  LibraryPresentation empty({}, LibrarySection::Favorites);
  require(empty.entries().empty() && empty.focus_index() == 0,
          "empty section should have stable state");
  require(!empty.handle(Action::Confirm).has_value(),
          "confirm should do nothing in an empty section");
  require(empty.handle(Action::Back).has_value(),
          "back should remain available in an empty section");
}

}  // namespace

int main() {
  try {
    menu_targets_map_to_sections();
    demo_library_is_sanitized_and_useful();
    filters_and_recent_order_are_deterministic();
    navigation_launch_and_unavailable_are_explicit();
    empty_section_is_safe();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
