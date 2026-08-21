#include "sprout/launcher/launcher_state.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using sprout::launcher::Action;
using sprout::launcher::EventType;
using sprout::launcher::LauncherState;
using sprout::launcher::ProfileRole;
using sprout::launcher::Screen;

std::vector<sprout::launcher::Profile> make_profiles(std::size_t count) {
  std::vector<sprout::launcher::Profile> profiles;
  for (std::size_t index = 0; index < count; ++index) {
    profiles.push_back({"profile-" + std::to_string(index),
                        "Profile " + std::to_string(index),
                        ProfileRole::Child, 0, "builtin:sprout"});
  }
  return profiles;
}

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void fixture_has_parent_and_child() {
  const auto profiles = sprout::launcher::make_demo_household();
  expect(profiles.size() == 2, "demo household should contain two profiles");
  expect(profiles[0].role == ProfileRole::Child, "first demo profile should be a child");
  expect(profiles[1].role == ProfileRole::Parent, "second demo profile should be a parent");
}

void profile_navigation_wraps() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Left);
  expect(state.focus_index() == 1, "left should wrap to the final profile");
  (void)state.handle(Action::Right);
  expect(state.focus_index() == 0, "right should wrap to the first profile");
}

void profile_selector_defaults_to_last_accessed_profile() {
  auto profiles = make_profiles(4);
  profiles[3].last_accessed = true;
  LauncherState state(std::move(profiles));
  expect(state.focus_index() == 3,
         "profile selector should focus the last accessed profile on launch");
}

void profile_vertical_navigation_moves_the_carousel() {
  LauncherState state(make_profiles(6));
  (void)state.handle(Action::Right);
  (void)state.handle(Action::Down);
  expect(state.focus_index() == 2,
         "down should advance the single profile carousel");
  (void)state.handle(Action::Up);
  expect(state.focus_index() == 1,
         "up should reverse the single profile carousel");
  (void)state.handle(Action::Up);
  expect(state.focus_index() == 0,
         "up should wrap the carousel backwards");
}

void child_profile_opens_child_home() {
  LauncherState state(sprout::launcher::make_demo_household());
  const auto event = state.handle(Action::Confirm);
  expect(event.has_value(), "profile selection should emit an event");
  expect(event->type == EventType::ProfileActivated, "selection should activate a profile");
  expect(event->profile_id == "child-alex", "child fixture ID should be preserved");
  expect(state.screen() == Screen::ChildHome, "child should open child home");
  expect(state.menu_items().size() == 4,
         "child Start menu should expose one concise role menu");
  expect(state.menu_items()[0] == "Game Dashboard",
         "child menu should return to the unified dashboard first");
  expect(state.menu_items()[1] == "Profile Picture" &&
             state.menu_items()[2] == "Background" &&
             state.menu_items()[3] == "Profile Select",
         "child menu should contain only games, appearance, and profile actions");
}

void parent_profile_opens_parent_home() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Right);
  const auto event = state.handle(Action::Confirm);
  expect(event.has_value(), "parent selection should emit an event");
  expect(event->profile_id == "parent-preview", "parent fixture ID should be preserved");
  expect(state.screen() == Screen::ParentHome, "parent should open parent home");
  expect(state.menu_items().size() == 5 &&
             state.menu_items()[0] == "Game Guide",
         "parent Start menu should expose one concise role menu");
}

void home_navigation_and_lifecycle_are_explicit() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Confirm);
  (void)state.handle(Action::Up);
  expect(state.focus_index() == 3, "up should wrap on the child Start menu");

  (void)state.handle(Action::Down);
  const auto invoked = state.handle(Action::Confirm);
  expect(invoked.has_value(), "menu confirmation should emit an event");
  expect(invoked->type == EventType::MenuItemInvoked, "menu event should identify invocation");
  expect(invoked->target == "Game Dashboard",
         "child menu event should carry its game target");

  const auto returned = state.handle(Action::Back);
  expect(returned.has_value(), "back should emit a return event");
  expect(returned->type == EventType::ReturnedToProfiles, "back should return to profiles");
  expect(state.screen() == Screen::ProfileSelect, "back should restore profile selection");
  expect(state.active_profile() == nullptr, "return should clear the active profile");
}

void menu_action_is_reserved_for_the_library_shell() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Confirm);
  const auto focus = state.focus_index();
  const auto event = state.handle(Action::Menu);
  expect(!event.has_value(), "menu action should be handled by the library shell");
  expect(state.screen() == Screen::ChildHome,
         "menu action should not leave the active profile menu");
  expect(state.focus_index() == focus,
         "menu action should not move the role menu focus");
}

void back_from_profiles_is_contained() {
  LauncherState state(sprout::launcher::make_demo_household());
  const auto event = state.handle(Action::Back);
  expect(!event.has_value(), "back at root must not emit an exit event");
  expect(state.screen() == Screen::ProfileSelect,
         "back at root must keep the profile selector visible");
}

}  // namespace

int main() {
  try {
    fixture_has_parent_and_child();
    profile_navigation_wraps();
    profile_selector_defaults_to_last_accessed_profile();
    profile_vertical_navigation_moves_the_carousel();
    child_profile_opens_child_home();
    parent_profile_opens_parent_home();
    home_navigation_and_lifecycle_are_explicit();
    menu_action_is_reserved_for_the_library_shell();
    back_from_profiles_is_contained();
  } catch (const std::exception& error) {
    std::cerr << "launcher state test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "launcher state tests passed\n";
  return EXIT_SUCCESS;
}
