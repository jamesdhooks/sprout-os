#include "sprout/launcher/parent_access_controller.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::AccessMoment;
using sprout::launcher::Action;
using sprout::launcher::LauncherState;
using sprout::launcher::ParentAccessController;
using sprout::launcher::ParentAccessEventType;
using sprout::launcher::ParentAccessStore;
using sprout::launcher::Screen;

const AccessMoment kToday{1'000, "2026-08-02"};

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-parent-controller-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

struct Fixture {
  TemporaryDirectory directory;
  ParentAccessStore access{directory.path() / "security.sqlite3",
                           directory.path() / "device.key"};
  LauncherState state{sprout::launcher::make_demo_household()};
  ParentAccessController controller{state, &access, "secret:parent-primary"};

  Fixture() { access.set_pin("secret:parent-primary", "2468"); }
};

void move_pin_to(ParentAccessController& controller, std::size_t target,
                 const AccessMoment& now) {
  while (controller.pin_prompt().focus_index() / 3 != target / 3) {
    (void)controller.handle(Action::Down, now);
  }
  while (controller.pin_prompt().focus_index() % 3 != target % 3) {
    (void)controller.handle(Action::Right, now);
  }
}

std::size_t digit_index(char digit) {
  return digit == '0' ? 10U : static_cast<std::size_t>(digit - '1');
}

std::optional<sprout::launcher::ParentAccessEvent> submit_pin(
    ParentAccessController& controller, std::string_view digits,
    const AccessMoment& now) {
  for (const char digit : digits) {
    move_pin_to(controller, digit_index(digit), now);
    (void)controller.handle(Action::Confirm, now);
  }
  move_pin_to(controller, 11, now);
  return controller.handle(Action::Confirm, now);
}

void parent_selection_requires_pin_and_grants_access() {
  Fixture fixture;
  (void)fixture.controller.handle(Action::Right, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.controller.has_pin_prompt() &&
             fixture.state.screen() == Screen::ProfileSelect,
         "locked parent selection should stay outside parent mode and request a PIN");
  (void)submit_pin(fixture.controller, "1357", kToday);
  expect(fixture.controller.has_pin_prompt() &&
             !fixture.controller.pin_prompt().error_message().empty(),
         "incorrect PIN should remain on a reset prompt");
  (void)submit_pin(fixture.controller, "2468", kToday);
  expect(!fixture.controller.has_pin_prompt() &&
             fixture.state.screen() == Screen::ParentHome &&
             fixture.access.is_unlocked(kToday.utc_seconds, kToday.local_date),
         "correct PIN should enter parent mode with a persisted grant");
}

void sensitive_actions_reauthenticate_and_manual_lock_revokes() {
  Fixture fixture;
  fixture.access.grant_until_end_of_day("secret:parent-primary", "2468",
                                        kToday.utc_seconds, kToday.local_date);
  (void)fixture.controller.handle(Action::Right, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.state.screen() == Screen::ParentHome,
         "valid grant should enter parent mode without another prompt");

  for (int index = 0; index < 4; ++index) {
    (void)fixture.controller.handle(Action::Down, kToday);
  }
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.controller.has_pin_prompt(),
         "family dashboard should require fresh authentication");
  const auto invoked = submit_pin(fixture.controller, "2468", kToday);
  expect(invoked.has_value() &&
             invoked->type == ParentAccessEventType::ActionInvoked &&
             invoked->target == "Family Dashboard",
         "fresh PIN should authorize only the pending sensitive action");

  (void)fixture.controller.handle(Action::Down, kToday);
  (void)fixture.controller.handle(Action::Down, kToday);
  (void)fixture.controller.handle(Action::Down, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.controller.has_pin_prompt(),
         "profile backup should require fresh authentication");
  const auto backup = submit_pin(fixture.controller, "2468", kToday);
  expect(backup.has_value() && backup->target == "Backup & Restore",
         "fresh PIN should authorize the pending backup operation");

  (void)fixture.controller.handle(Action::Down, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.state.screen() == Screen::ProfileSelect &&
             !fixture.access.is_unlocked(kToday.utc_seconds, kToday.local_date),
         "manual lock should revoke immediately and leave parent mode");
}

void grant_expiry_returns_to_profile_selection() {
  Fixture fixture;
  fixture.access.grant_until_end_of_day("secret:parent-primary", "2468",
                                        kToday.utc_seconds, kToday.local_date);
  (void)fixture.controller.handle(Action::Right, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  const AccessMoment tomorrow{2'000, "2026-08-03"};
  (void)fixture.controller.handle(Action::Down, tomorrow);
  expect(fixture.state.screen() == Screen::ProfileSelect,
         "expired grant should leave parent mode before processing another action");
}

void subview_access_revalidates_parent_grant() {
  Fixture fixture;
  fixture.access.grant_until_end_of_day("secret:parent-primary", "2468",
                                        kToday.utc_seconds, kToday.local_date);
  (void)fixture.controller.handle(Action::Right, kToday);
  (void)fixture.controller.handle(Action::Confirm, kToday);
  expect(fixture.controller.ensure_active_profile_access(kToday),
         "active parent subview should accept the current grant");

  const AccessMoment tomorrow{2'000, "2026-08-03"};
  expect(!fixture.controller.ensure_active_profile_access(tomorrow) &&
             fixture.state.screen() == Screen::ProfileSelect,
         "expired parent subview should fail closed before its next action");
}

void system_exit_requires_parent_authorization() {
  Fixture fixture;

  const auto gated = fixture.controller.request_exit(kToday);
  expect(!gated.has_value() && fixture.controller.has_pin_prompt(),
         "system exit at profile selection should open the parent PIN prompt");
  const auto wrong = submit_pin(fixture.controller, "1357", kToday);
  expect(!wrong.has_value() && fixture.controller.has_pin_prompt(),
         "an incorrect exit PIN must remain gated");
  const auto authorized = submit_pin(fixture.controller, "2468", kToday);
  expect(authorized.has_value() &&
             authorized->type == ParentAccessEventType::ExitRequested,
         "a correct parent PIN should authorize exactly one system exit");
}

void child_exit_is_gated_but_active_parent_may_exit() {
  Fixture child;
  (void)child.controller.handle(Action::Confirm, kToday);
  expect(child.state.screen() == Screen::ChildHome,
         "child fixture should activate the child profile");
  const auto child_exit = child.controller.request_exit(kToday);
  expect(!child_exit.has_value() && child.controller.has_pin_prompt(),
         "an active child profile must require the parent PIN to exit");

  Fixture parent;
  parent.access.grant_until_end_of_day("secret:parent-primary", "2468",
                                       kToday.utc_seconds, kToday.local_date);
  (void)parent.controller.handle(Action::Right, kToday);
  (void)parent.controller.handle(Action::Confirm, kToday);
  expect(parent.state.screen() == Screen::ParentHome,
         "parent fixture should activate an authenticated parent profile");
  const auto parent_exit = parent.controller.request_exit(kToday);
  expect(parent_exit.has_value() &&
             parent_exit->type == ParentAccessEventType::ExitRequested &&
             !parent.controller.has_pin_prompt(),
         "an actively authenticated parent profile may exit directly");
}

}  // namespace

int main() {
  try {
    parent_selection_requires_pin_and_grants_access();
    sensitive_actions_reauthenticate_and_manual_lock_revokes();
    grant_expiry_returns_to_profile_selection();
    subview_access_revalidates_parent_grant();
    system_exit_requires_parent_authorization();
    child_exit_is_gated_but_active_parent_may_exit();
  } catch (const std::exception& error) {
    std::cerr << "parent access controller test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "parent access controller tests passed\n";
  return EXIT_SUCCESS;
}
