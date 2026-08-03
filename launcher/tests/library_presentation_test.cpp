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
using sprout::launcher::NativeLaunchTarget;
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
      .id = id,
      .title = std::move(title),
      .platform_label = system == OnionSystem::GameBoy ? "GB" : "SFC",
      .launch_target = sprout::launcher::EmulatedLaunchTarget{
          .item_id = std::move(id),
          .system = system,
          .rom_path = std::filesystem::temp_directory_path() / "fixture.rom",
          .launch_allowed = allowed,
      },
      .child_visible = false,
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
  require(library_section_for_menu_target("Sprout Arcade") ==
              LibrarySection::Arcade,
          "Sprout Arcade should open the native-game section");
}

void demo_library_is_sanitized_and_useful() {
  const auto demo = sprout::launcher::make_demo_library();
  require(demo.size() == 5, "demo library should remain small and deterministic");
  const auto& target = std::get<sprout::launcher::EmulatedLaunchTarget>(
      demo[0].launch_target);
  require(sprout::launcher::starts_with(demo[0].id, "preview:") &&
              target.rom_path.is_absolute(),
          "demo entries should be clearly synthetic typed targets");
}

void filters_and_recent_order_are_deterministic() {
  LibraryPresentation recent(entries(), LibrarySection::Recent);
  require(recent.entries().size() == 2,
          "recent section should contain ranked entries only");
  require(recent.entries()[0].id == "game-b" &&
              recent.entries()[1].id == "game-a",
          "recent section should sort by explicit rank");

  LibraryPresentation favorites(entries(), LibrarySection::Favorites);
  require(favorites.entries().size() == 2,
          "favorites section should contain favorite entries only");
  require(favorites.entries()[0].id == "game-b" &&
              favorites.entries()[1].id == "game-c",
          "favorites should preserve catalogue order");

  LibraryPresentation all(entries(), LibrarySection::All);
  require(all.entries().size() == 3, "all section should retain every entry");

  auto native_entries = entries();
  native_entries.push_back(LibraryEntry{
      .id = "arcade:sprout.snake",
      .title = "Snake",
      .platform_label = "ARCADE",
      .launch_target = NativeLaunchTarget{
          .item_id = "arcade:sprout.snake",
          .package_root = std::filesystem::temp_directory_path() / "snake",
          .profile_id = "child-alex",
          .seed = 7,
          .launch_allowed = true,
      },
      .child_visible = true,
      .launch_allowed = true,
  });
  LibraryPresentation arcade(std::move(native_entries), LibrarySection::Arcade);
  require(arcade.entries().size() == 1 &&
              arcade.entries()[0].id == "arcade:sprout.snake",
          "Arcade should contain only native launch targets");
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
  const auto& launch = std::get<sprout::launcher::EmulatedLaunchTarget>(
      *event->launch_target);
  require(launch.item_id == "game-a" &&
              launch.system == OnionSystem::GameBoy &&
              launch.launch_allowed,
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
