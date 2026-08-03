#include "sprout/launcher/native_launch_adapter.hpp"

#include "sprout/runtime/package.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sprout::launcher {
namespace {

NativeLaunchResult result(const NativeLaunchTarget& target,
                          NativeLaunchOutcome outcome, std::string detail,
                          std::optional<int> exit_code = std::nullopt) {
  return {
      .outcome = outcome,
      .item_id = target.item_id,
      .exit_code = exit_code,
      .detail = std::move(detail),
  };
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

bool valid_profile_id(std::string_view value) {
  return !value.empty() && value.size() <= 64 &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return std::isalnum(character) != 0 || character == '-' ||
                  character == '_';
         });
}

bool within(const std::filesystem::path& path,
            const std::filesystem::path& root) {
  auto path_part = path.begin();
  for (auto root_part = root.begin(); root_part != root.end();
       ++root_part, ++path_part) {
    if (path_part == path.end() || *path_part != *root_part) {
      return false;
    }
  }
  return true;
}

}  // namespace

NativeLaunchAdapter::NativeLaunchAdapter(
    std::filesystem::path runtime_executable,
    std::filesystem::path storage_root, LaunchProcess& process)
    : runtime_executable_(std::move(runtime_executable)),
      storage_root_(std::move(storage_root)),
      process_(process) {}

NativeLaunchResult NativeLaunchAdapter::launch(
    const NativeLaunchTarget& target, NativeLaunchMode mode) const {
  if (!target.launch_allowed) {
    return result(target, NativeLaunchOutcome::PolicyDenied,
                  "The active profile does not allow this native game");
  }
  if (target.item_id.empty() || target.package_root.empty() ||
      !target.package_root.is_absolute() || !valid_profile_id(target.profile_id) ||
      target.seed == 0) {
    return result(target, NativeLaunchOutcome::InvalidTarget,
                  "The native launch target is incomplete");
  }

  sprout::runtime::PackageManifest package;
  try {
    package = sprout::runtime::load_package(target.package_root);
  } catch (const std::exception& error) {
    return result(target, NativeLaunchOutcome::PackageUnavailable, error.what());
  }
  if (target.item_id != "arcade:" + package.id) {
    return result(target, NativeLaunchOutcome::InvalidTarget,
                  "The package identity changed after discovery");
  }

  std::error_code error;
  const auto runtime =
      std::filesystem::canonical(runtime_executable_, error);
  if (error || !std::filesystem::is_regular_file(runtime, error) || error) {
    return result(target, NativeLaunchOutcome::RuntimeUnavailable,
                  "The configured Sprout Runtime executable is unavailable");
  }

  std::filesystem::create_directories(storage_root_, error);
  const auto storage_root =
      std::filesystem::canonical(storage_root_, error);
  if (error || !std::filesystem::is_directory(storage_root, error) || error) {
    return result(target, NativeLaunchOutcome::InvalidTarget,
                  "The native-game storage root is unavailable");
  }
  const auto requested_profile_storage = storage_root / target.profile_id;
  std::filesystem::create_directories(requested_profile_storage, error);
  const auto profile_storage =
      std::filesystem::canonical(requested_profile_storage, error);
  if (error || !within(profile_storage, storage_root)) {
    return result(target, NativeLaunchOutcome::InvalidTarget,
                  "The profile storage target is invalid");
  }

  std::vector<std::string> arguments{
      "--package", path_as_utf8(package.root), "--storage",
      path_as_utf8(profile_storage), "--seed", std::to_string(target.seed)};
  if (mode == NativeLaunchMode::SmokeTest) {
    arguments.push_back("--smoke-test");
  }
  const auto process = process_.run(runtime, arguments);
  if (!process.started) {
    return result(target, NativeLaunchOutcome::ProcessStartFailed,
                  process.detail);
  }
  if (!process.exit_code.has_value() || *process.exit_code != 0) {
    const auto detail = process.detail.empty() && process.exit_code.has_value()
                            ? "Native game exited with status " +
                                  std::to_string(*process.exit_code)
                            : process.detail;
    return result(target, NativeLaunchOutcome::AbnormalExit, detail,
                  process.exit_code);
  }
  return result(target, NativeLaunchOutcome::Completed, {}, process.exit_code);
}

}  // namespace sprout::launcher
