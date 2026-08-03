#include "sprout/launcher/daily_time_policy.hpp"
#include "sprout/launcher/library_presentation.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/local_library.hpp"
#include "sprout/launcher/onion_launch_adapter.hpp"
#include "sprout/launcher/parent_access_controller.hpp"
#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/profile_archive.hpp"
#include "sprout/launcher/profile_repository.hpp"
#include "sprout/launcher/recovery_presentation.hpp"
#include "sprout/launcher/setup_wizard.hpp"
#include "sprout/launcher/startup_health.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace sprout::launcher;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

class TemporaryRoot {
 public:
  TemporaryRoot() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-mvp-integration-" + std::to_string(suffix));
    std::filesystem::create_directories(path_ / "data");
    std::filesystem::create_directories(path_ / "sd" / "Emu" / "GB");
    std::filesystem::create_directories(path_ / "sd" / "Emu" / "SFC");
    std::filesystem::create_directories(path_ / "sd" / "Roms" / "GB");
    std::filesystem::create_directories(path_ / "sd" / "Roms" / "SFC");
  }

  ~TemporaryRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path& path() const noexcept { return path_; }

  void write(std::filesystem::path relative, std::string_view contents = {}) {
    const auto destination = path_ / std::move(relative);
    std::filesystem::create_directories(destination.parent_path());
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!stream) {
      throw std::runtime_error("Could not write MVP integration fixture");
    }
  }

 private:
  std::filesystem::path path_;
};

class RecordingProcess final : public LaunchProcess {
 public:
  ProcessResult run(const std::filesystem::path& executable,
                    const std::vector<std::string>& arguments) override {
    executables.push_back(executable);
    argument_sets.push_back(arguments);
    return {.started = true, .exit_code = 0, .detail = {}};
  }

  std::vector<std::filesystem::path> executables;
  std::vector<std::vector<std::string>> argument_sets;
};

NewProfile parent_profile() {
  return {
      .id = "parent-primary",
      .display_name = "Parent",
      .role = ProfileRole::Parent,
      .avatar_ref = "builtin:owl",
      .save_namespace = "parent-primary",
      .content_policy_ref = std::nullopt,
      .time_policy_ref = std::nullopt,
  };
}

NewProfile child_profile() {
  return {
      .id = "child-primary",
      .display_name = "Child",
      .role = ProfileRole::Child,
      .avatar_ref = "builtin:fox",
      .save_namespace = "child-primary",
      .content_policy_ref = "content:child-default",
      .time_policy_ref = "time:child-default",
  };
}

TimePolicySample sample(std::uint64_t monotonic_milliseconds,
                        std::int64_t utc_seconds) {
  return {
      .monotonic_milliseconds = monotonic_milliseconds,
      .utc_seconds = utc_seconds,
      .local_date = "2026-08-02",
  };
}

LibraryEntry entry_for(const EmulatedLibraryItem& item, bool favorite,
                       std::optional<std::size_t> recent_rank) {
  return {
      .item = item,
      .favorite = favorite,
      .recent_rank = recent_rank,
      .launch_allowed = true,
  };
}

EmulatedLaunchTarget select_first(std::vector<LibraryEntry> entries,
                                  LibrarySection section) {
  LibraryPresentation presentation(std::move(entries), section);
  const auto selected = presentation.handle(Action::Confirm);
  expect(selected.has_value() &&
             selected->type == LibraryPresentationEventType::LaunchRequested &&
             selected->launch_target.has_value(),
         "library fixture should produce a typed launch request");
  return *selected->launch_target;
}

void complete_host_journey() {
  TemporaryRoot root;
  const auto configuration_path = root.path() / "config";
  const auto profiles_path = root.path() / "data" / "profiles.sqlite3";
  const auto policy_path = root.path() / "data" / "time-policy.sqlite3";
  const auto health_path = root.path() / "data" / "startup-health.sqlite3";

  std::uint64_t interrupted_attempt = 0;
  {
    StartupHealthStore health(health_path);
    const auto attempt = health.begin_startup();
    interrupted_attempt = attempt.attempt_id;
    ConfigurationStore configuration(configuration_path);
    ProfileRepository profiles(profiles_path);
    DailyTimePolicyStore policy(policy_path);
    SetupWizard setup(configuration, profiles, &policy);
    setup.skip_current_step();
    expect(setup.current_step() == SetupStep::Locale,
           "first setup boundary should persist before interruption");
  }

  std::uint64_t ready_attempt = 0;
  {
    StartupHealthStore health(health_path);
    const auto attempt = health.begin_startup();
    ready_attempt = attempt.attempt_id;
    expect(attempt.attempt_id > interrupted_attempt &&
               attempt.consecutive_failures == 1,
           "reopen should count the unfinished startup once");
    ConfigurationStore configuration(configuration_path);
    ProfileRepository profiles(profiles_path);
    DailyTimePolicyStore policy(policy_path);
    SetupWizard setup(configuration, profiles, &policy);
    expect(setup.current_step() == SetupStep::Locale,
           "setup should resume from its durable step");
    setup.configure_locale({.language = "en", .region = "CA"});
    setup.continue_offline();
    setup.create_parent(parent_profile());
    setup.set_parent_credential_ref("secret:parent-primary");
    setup.create_child(child_profile());
    setup.skip_current_step();  // Avatars
    setup.skip_current_step();  // Library
    setup.skip_current_step();  // Child defaults
    setup.skip_current_step();  // Connectors
    setup.finish();
    expect(setup.current_step() == SetupStep::Complete &&
               profiles.list_profiles(false).size() == 2 &&
               policy.find_daily_allowance_seconds("child-primary") ==
                   kDefaultChildDailyAllowanceSeconds,
           "offline setup should persist both profiles and child allowance");
    health.mark_ready(attempt.attempt_id);
  }

  {
    StartupHealthStore health(health_path);
    const auto attempt = health.begin_startup();
    expect(attempt.attempt_id > ready_attempt &&
               attempt.consecutive_failures == 0,
           "ready setup should clear the prior failure on the next start");
    health.mark_ready(attempt.attempt_id);
  }

  const auto security_path = root.path() / "data" / "security.sqlite3";
  const auto key_path = root.path() / "secrets" / "device-access.key";
  {
    ParentAccessStore access(security_path, key_path);
    access.set_pin("secret:parent-primary", "2468");
    access.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                  "2026-08-02");
    LauncherState state({
        {.id = "child-primary", .display_name = "Child",
         .role = ProfileRole::Child, .accent_rgb = 0x69b578,
         .avatar_ref = "builtin:fox"},
        {.id = "parent-primary", .display_name = "Parent",
         .role = ProfileRole::Parent, .accent_rgb = 0xe0a458,
         .avatar_ref = "builtin:owl"},
    });
    ParentAccessController controller(state, &access,
                                      "secret:parent-primary");
    (void)controller.handle(Action::Right, {1'000, "2026-08-02"});
    (void)controller.handle(Action::Confirm, {1'000, "2026-08-02"});
    expect(state.screen() == Screen::ParentHome &&
               !controller.has_pin_prompt(),
           "persisted end-of-day grant should enter parent mode");
    for (int index = 0; index < 6; ++index) {
      (void)controller.handle(Action::Down, {1'000, "2026-08-02"});
    }
    (void)controller.handle(Action::Confirm, {1'000, "2026-08-02"});
    expect(state.screen() == Screen::ProfileSelect &&
               !access.is_unlocked(1'000, "2026-08-02"),
           "manual parent lock should revoke the persisted grant immediately");
  }
  {
    ParentAccessStore reopened(security_path, key_path);
    expect(!reopened.is_unlocked(1'000, "2026-08-02"),
           "manual parent lock should remain revoked after reopening storage");
  }

  root.write("sd/Emu/GB/launch.sh");
  root.write("sd/Emu/SFC/launch.sh");
  root.write("sd/Roms/GB/Family Test.gb");
  root.write("sd/Roms/SFC/Family Test.sfc");
  LocalLibraryScanner scanner(root.path() / "sd");
  const auto scan = scanner.discover();
  expect(scan.warnings.empty() && scan.items.size() == 2,
         "synthetic card should discover one GB and one SNES item");
  const auto gb = *std::find_if(scan.items.begin(), scan.items.end(),
                               [](const auto& item) {
                                 return item.system == OnionSystem::GameBoy;
                               });
  const auto snes = *std::find_if(scan.items.begin(), scan.items.end(),
                                 [](const auto& item) {
                                   return item.system ==
                                          OnionSystem::SuperNintendo;
                                 });
  const auto gb_target =
      select_first({entry_for(gb, false, 0)}, LibrarySection::Recent);
  const auto snes_target =
      select_first({entry_for(snes, true, std::nullopt)},
                   LibrarySection::Favorites);
  RecordingProcess process;
  OnionLaunchAdapter adapter(root.path() / "sd", process);
  expect(adapter.launch(gb_target).completed() &&
             adapter.launch(snes_target).completed() &&
             process.executables.size() == 2 &&
             process.argument_sets[0].size() == 1 &&
             process.argument_sets[1].size() == 1,
         "GB recent and SNES favorite should reach fixed typed adapter calls");

  {
    DailyTimePolicyStore policy(policy_path);
    policy.set_daily_allowance("child-primary", 60);
    const auto began = policy.begin_session("child-primary", "session-1",
                                            gb_target.item_id,
                                            sample(1'000, 1'000));
    const auto expired = policy.checkpoint_session("session-1",
                                                   sample(61'000, 1'060));
    expect(began.launch_allowed && expired.status.expired &&
               expired.save_and_exit_required && !expired.launch_allowed,
           "child time expiry should request normal save and exit");
  }
  {
    DailyTimePolicyStore policy(policy_path);
    const auto blocked = policy.begin_session("child-primary", "session-2",
                                              snes_target.item_id,
                                              sample(62'000, 1'061));
    expect(!blocked.launch_allowed,
           "reopened policy store should block another child launch");
  }

  root.write("saves/child-primary/sentinel.sav", "save-data");
  const auto archive_path = root.path() / "exports" / "child.sprout-profile";
  std::filesystem::create_directories(archive_path.parent_path());
  {
    ProfileRepository profiles(profiles_path);
    DailyTimePolicyStore policy(policy_path);
    ProfileArchiveService archives(profiles, policy,
                                   root.path() / "data" / "profile-images");
    (void)archives.export_profile("child-primary", archive_path);
  }
  {
    std::filesystem::create_directories(root.path() / "restore");
    ProfileRepository profiles(root.path() / "restore" / "profiles.sqlite3");
    DailyTimePolicyStore policy(root.path() / "restore" /
                                "time-policy.sqlite3");
    ProfileArchiveService archives(profiles, policy,
                                   root.path() / "restore" / "profile-images");
    const auto restored = archives.restore_profile(archive_path);
    expect(restored.profile_id == "child-primary" &&
               profiles.find_profile("child-primary").has_value() &&
               policy.find_daily_allowance_seconds("child-primary") == 60,
           "portable child profile and allowance should restore after reopen");
  }
  expect(std::filesystem::file_size(
             root.path() / "saves" / "child-primary" / "sentinel.sav") == 9,
         "profile portability should not mutate saves");

  for (int failure = 0; failure < 3; ++failure) {
    StartupHealthStore health(health_path);
    const auto attempt = health.begin_startup();
    expect(!attempt.recovery_required,
           "first three unfinished starts should not enter recovery early");
  }
  StartupDecision recovery_attempt{};
  {
    StartupHealthStore health(health_path);
    recovery_attempt = health.begin_startup();
    expect(recovery_attempt.recovery_required &&
               recovery_attempt.consecutive_failures == 3,
           "fourth start should route to recovery");
  }
  {
    ConfigurationStore configuration(configuration_path);
    RecoveryPresentation recovery(configuration, recovery_attempt.attempt_id);
    (void)recovery.handle(Action::Up);
    (void)recovery.handle(Action::Up);
    (void)recovery.handle(Action::Confirm);
    (void)recovery.handle(Action::Left);
    expect(recovery.handle(Action::Confirm) ==
               RecoveryPresentationEvent::ConfigurationChanged,
           "confirmed last-known-good recovery should reactivate configuration");
  }
  expect(std::filesystem::file_size(
             root.path() / "saves" / "child-primary" / "sentinel.sav") == 9,
         "configuration recovery should preserve saves");
  {
    StartupHealthStore health(health_path);
    health.mark_ready(recovery_attempt.attempt_id);
    const auto next = health.begin_startup();
    expect(next.consecutive_failures == 0 && !next.recovery_required,
           "ordinary readiness after recovery should clear failure state");
    health.mark_ready(next.attempt_id);
  }
}

}  // namespace

int main() {
  try {
    complete_host_journey();
  } catch (const std::exception& error) {
    std::cerr << "MVP integration test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "MVP host integration journey passed\n";
  return EXIT_SUCCESS;
}
