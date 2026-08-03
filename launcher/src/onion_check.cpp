#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/daily_time_policy.hpp"
#include "sprout/launcher/library_presentation.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/local_library.hpp"
#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/profile_archive.hpp"
#include "sprout/launcher/profile_repository.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Arguments {
  std::filesystem::path data_directory;
  std::optional<std::filesystem::path> sd_card_root;
};

Arguments parse_arguments(int count, char* values[]) {
  Arguments result;
  for (int index = 1; index < count; ++index) {
    const std::string_view argument(values[index]);
    if (argument == "--data-dir" && index + 1 < count) {
      result.data_directory = values[++index];
    } else if (argument == "--sd-root" && index + 1 < count) {
      result.sd_card_root = std::filesystem::path(values[++index]);
    } else {
      throw std::invalid_argument("Unsupported or incomplete diagnostic argument");
    }
  }
  if (result.data_directory.empty()) {
    throw std::invalid_argument("--data-dir must name a new diagnostic directory");
  }
  return result;
}

void prepare_diagnostic_directory(const std::filesystem::path& directory) {
  std::error_code error;
  if (std::filesystem::exists(directory, error)) {
    if (error || !std::filesystem::is_directory(directory, error) || error ||
        !std::filesystem::is_empty(directory, error) || error) {
      throw std::invalid_argument(
          "Diagnostic data directory must be absent or empty");
    }
    return;
  }
  if (!std::filesystem::create_directories(directory, error) || error) {
    throw std::runtime_error("Diagnostic data directory could not be created");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    const auto arguments = parse_arguments(argc, argv);
    prepare_diagnostic_directory(arguments.data_directory);

    sprout::launcher::ProfileRepository profiles(
        arguments.data_directory / "profiles.sqlite3");
    profiles.create_profile(sprout::launcher::NewProfile{
        .id = "diagnostic-parent",
        .display_name = "Diagnostic Parent",
        .role = sprout::launcher::ProfileRole::Parent,
        .avatar_ref = "builtin:fox",
        .save_namespace = "diagnostic-saves",
        .content_policy_ref = std::nullopt,
        .time_policy_ref = std::nullopt,
        .preferences_json = "{}",
    });
    profiles.create_profile(sprout::launcher::NewProfile{
        .id = "diagnostic-child",
        .display_name = "Diagnostic Child",
        .role = sprout::launcher::ProfileRole::Child,
        .avatar_ref = "builtin:sprout",
        .save_namespace = "diagnostic-child-saves",
        .content_policy_ref = "content:child-default",
        .time_policy_ref = "time:child-default",
        .preferences_json = "{}",
    });

    sprout::launcher::ConfigurationStore configuration(
        arguments.data_directory / "config");
    auto local_configuration = sprout::launcher::LocalConfiguration{};
    local_configuration.next_setup_step = sprout::launcher::SetupStep::Complete;
    local_configuration.household_id = "diagnostic-household";
    local_configuration.device_id = "diagnostic-device";
    (void)configuration.save(std::move(local_configuration));

    std::filesystem::create_directories(arguments.data_directory / "secrets");
    sprout::launcher::ParentAccessStore access(
        arguments.data_directory / "security.sqlite3",
        arguments.data_directory / "secrets" / "device-access.key");
    const auto hash_started = std::chrono::steady_clock::now();
    access.set_pin("secret:diagnostic-parent", "2468");
    const auto hash_finished = std::chrono::steady_clock::now();
    if (!access.verify_pin("secret:diagnostic-parent", "2468")) {
      throw std::runtime_error("Parent PIN verification failed");
    }

    sprout::launcher::DailyTimePolicyStore time_policy(
        arguments.data_directory / "time-policy.sqlite3");
    time_policy.set_daily_allowance("diagnostic-child", 45 * 60);
    const auto time_started = time_policy.begin_session(
        "diagnostic-child", "diagnostic-session", "diagnostic-game",
        sprout::launcher::TimePolicySample{
            .monotonic_milliseconds = 1'000,
            .utc_seconds = 1'000,
            .local_date = "2026-01-01",
        });
    const auto time_paused = time_policy.pause_session(
        "diagnostic-session",
        sprout::launcher::TimePolicySample{
            .monotonic_milliseconds = 2'000,
            .utc_seconds = 1'001,
            .local_date = "2026-01-01",
        });
    if (!time_started.launch_allowed ||
        time_paused.status.used_milliseconds != 1'000) {
      throw std::runtime_error("Daily time-policy check failed");
    }

    sprout::launcher::ProfileArchiveService archives(
        profiles, time_policy, arguments.data_directory / "profile-images");
    const auto archive_path =
        arguments.data_directory / "diagnostic-child.sprout-profile";
    (void)archives.export_profile("diagnostic-child", archive_path);
    const auto restore_directory = arguments.data_directory / "restore-check";
    std::filesystem::create_directories(restore_directory);
    sprout::launcher::ProfileRepository restored_profiles(
        restore_directory / "profiles.sqlite3");
    sprout::launcher::DailyTimePolicyStore restored_time_policy(
        restore_directory / "time-policy.sqlite3");
    sprout::launcher::ProfileArchiveService restored_archives(
        restored_profiles, restored_time_policy,
        restore_directory / "profile-images");
    (void)restored_archives.restore_profile(archive_path);
    if (!restored_profiles.find_profile("diagnostic-child").has_value() ||
        restored_time_policy.find_daily_allowance_seconds("diagnostic-child") !=
            45U * 60U) {
      throw std::runtime_error("Profile archive round-trip check failed");
    }

    sprout::launcher::LauncherState state(
        sprout::launcher::make_demo_household());
    (void)state.handle(sprout::launcher::Action::Confirm);
    sprout::launcher::LibraryPresentation library(
        sprout::launcher::make_demo_library(),
        sprout::launcher::LibrarySection::All);
    const auto launch = library.handle(sprout::launcher::Action::Confirm);
    if (!launch.has_value() || !launch->launch_target.has_value()) {
      throw std::runtime_error("Typed launch request check failed");
    }

    std::cout << "onion-baseline=v4.3.1-1@7dfc008b851398dcfe57819519efe5f958c77f65\n";
    std::cout << "toolchain=arm-linux-gnueabihf-gcc-8.3.0@sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4\n";
    std::cout << "profile-store=ok\nconfiguration-store=ok\nparent-access=ok\n";
    std::cout << "daily-time-policy=ok\n";
    std::cout << "profile-archive=ok\n";
    std::cout << "argon2-set-pin-ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     hash_finished - hash_started)
                     .count()
              << '\n';
    std::cout << "typed-launch-request=" << launch->launch_target->item_id
              << '\n';

    if (arguments.sd_card_root.has_value()) {
      const auto scan = sprout::launcher::LocalLibraryScanner(
                            *arguments.sd_card_root)
                            .discover();
      std::cout << "discovered-items=" << scan.items.size() << '\n';
      for (const auto& warning : scan.warnings) {
        std::cout << "library-warning=" << warning << '\n';
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "onion-check: " << error.what() << '\n';
    return 1;
  }
}
