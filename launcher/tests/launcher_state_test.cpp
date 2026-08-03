#include "sprout/launcher/launcher_state.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::EventType;
using sprout::launcher::LauncherState;
using sprout::launcher::ProfileRole;
using sprout::launcher::Screen;

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

void child_profile_opens_child_home() {
  LauncherState state(sprout::launcher::make_demo_household());
  const auto event = state.handle(Action::Confirm);
  expect(event.has_value(), "profile selection should emit an event");
  expect(event->type == EventType::ProfileActivated, "selection should activate a profile");
  expect(event->profile_id == "child-alex", "child fixture ID should be preserved");
  expect(state.screen() == Screen::ChildHome, "child should open child home");
  expect(state.menu_items().size() == 6, "child home should expose its focused preview menu");
  expect(state.menu_items()[3] == "Sprout Arcade",
         "child home should expose the local Arcade collection");
}

void parent_profile_opens_parent_home() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Right);
  const auto event = state.handle(Action::Confirm);
  expect(event.has_value(), "parent selection should emit an event");
  expect(event->profile_id == "parent-preview", "parent fixture ID should be preserved");
  expect(state.screen() == Screen::ParentHome, "parent should open parent home");
  expect(state.menu_items().size() == 10, "parent home should expose its focused preview menu");
  expect(state.menu_items()[3] == "Sprout Arcade",
         "parent home should expose the local Arcade collection");
  expect(state.menu_items()[5] == "Profile Settings",
         "parent home should expose profile image controls");
  expect(state.menu_items()[7] == "Backup & Restore",
         "parent home should expose portable profile backup");
  expect(state.menu_items()[8] == "Lock Parent Access",
         "parent home should expose explicit manual lock");
}

void home_navigation_and_lifecycle_are_explicit() {
  LauncherState state(sprout::launcher::make_demo_household());
  (void)state.handle(Action::Confirm);
  (void)state.handle(Action::Up);
  expect(state.focus_index() == 5, "up should wrap on child home");

  (void)state.handle(Action::Down);
  const auto invoked = state.handle(Action::Confirm);
  expect(invoked.has_value(), "menu confirmation should emit an event");
  expect(invoked->type == EventType::MenuItemInvoked, "menu event should identify invocation");
  expect(invoked->target == "Continue", "menu event should carry a domain target");

  const auto returned = state.handle(Action::Back);
  expect(returned.has_value(), "back should emit a return event");
  expect(returned->type == EventType::ReturnedToProfiles, "back should return to profiles");
  expect(state.screen() == Screen::ProfileSelect, "back should restore profile selection");
  expect(state.active_profile() == nullptr, "return should clear the active profile");
}

void back_from_profiles_requests_exit() {
  LauncherState state(sprout::launcher::make_demo_household());
  const auto event = state.handle(Action::Back);
  expect(event.has_value(), "back at root should emit an event");
  expect(event->type == EventType::ExitRequested, "back at root should request application exit");
}

}  // namespace

int main() {
  try {
    fixture_has_parent_and_child();
    profile_navigation_wraps();
    child_profile_opens_child_home();
    parent_profile_opens_parent_home();
    home_navigation_and_lifecycle_are_explicit();
    back_from_profiles_requests_exit();
  } catch (const std::exception& error) {
    std::cerr << "launcher state test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "launcher state tests passed\n";
  return EXIT_SUCCESS;
}
