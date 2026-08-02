#include "sprout/launcher/onion_launch_adapter.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#endif

namespace sprout::launcher {
namespace {

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
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

LaunchResult result(const EmulatedLaunchTarget& target, LaunchOutcome outcome,
                    std::string detail,
                    std::optional<int> exit_code = std::nullopt) {
  return LaunchResult{
      .outcome = outcome,
      .item_id = target.item_id,
      .exit_code = exit_code,
      .detail = std::move(detail),
  };
}

}  // namespace

std::optional<OnionSystemContract> onion_system_contract(OnionSystem system) {
  static constexpr std::array<std::string_view, 6> game_boy_extensions{
      ".bin", ".dmg", ".gb", ".gbc", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 7> snes_extensions{
      ".sfc", ".smc", ".fig", ".bs", ".st", ".zip", ".7z"};
  switch (system) {
    case OnionSystem::GameBoy:
      return OnionSystemContract{
          .id = "GB",
          .rom_directory = "Roms/GB",
          .launcher = "Emu/GB/launch.sh",
          .extensions = game_boy_extensions,
      };
    case OnionSystem::SuperNintendo:
      return OnionSystemContract{
          .id = "SFC",
          .rom_directory = "Roms/SFC",
          .launcher = "Emu/SFC/launch.sh",
          .extensions = snes_extensions,
      };
  }
  return std::nullopt;
}

ProcessResult OnionLaunchProcess::run(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) {
#ifdef _WIN32
  static_cast<void>(executable);
  static_cast<void>(arguments);
  return {
      .started = false,
      .exit_code = std::nullopt,
      .detail = "Onion launch execution is available only on the target Linux system",
  };
#else
  std::vector<std::string> owned_arguments;
  owned_arguments.reserve(arguments.size() + 1);
  owned_arguments.push_back(executable.string());
  owned_arguments.insert(owned_arguments.end(), arguments.begin(), arguments.end());

  std::vector<char*> argument_pointers;
  argument_pointers.reserve(owned_arguments.size() + 1);
  for (auto& argument : owned_arguments) {
    argument_pointers.push_back(argument.data());
  }
  argument_pointers.push_back(nullptr);

  pid_t process_id{};
  const auto spawn_result = posix_spawn(
      &process_id, executable.c_str(), nullptr, nullptr,
      argument_pointers.data(), environ);
  if (spawn_result != 0) {
    return {
        .started = false,
        .exit_code = std::nullopt,
        .detail = std::error_code(spawn_result, std::generic_category()).message(),
    };
  }

  int status{};
  while (waitpid(process_id, &status, 0) == -1) {
    if (errno != EINTR) {
      return {
          .started = true,
          .exit_code = std::nullopt,
          .detail = std::error_code(errno, std::generic_category()).message(),
      };
    }
  }
  if (WIFEXITED(status)) {
    return {
        .started = true,
        .exit_code = WEXITSTATUS(status),
        .detail = {},
    };
  }
  if (WIFSIGNALED(status)) {
    return {
        .started = true,
        .exit_code = 128 + WTERMSIG(status),
        .detail = "Onion launcher terminated by a signal",
    };
  }
  return {
      .started = true,
      .exit_code = std::nullopt,
      .detail = "Onion launcher ended without an exit status",
  };
#endif
}

OnionLaunchAdapter::OnionLaunchAdapter(std::filesystem::path sd_card_root,
                                       LaunchProcess& process)
    : sd_card_root_(std::move(sd_card_root)), process_(process) {}

LaunchResult OnionLaunchAdapter::launch(
    const EmulatedLaunchTarget& target) const {
  if (!target.launch_allowed) {
    return result(target, LaunchOutcome::PolicyDenied,
                  "The active profile does not allow this item");
  }
  if (target.item_id.empty() || target.rom_path.empty() ||
      !target.rom_path.is_absolute()) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The library item does not contain a valid local ROM target");
  }

  const auto contract = onion_system_contract(target.system);
  if (!contract.has_value()) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The Onion system is not supported by this adapter version");
  }

  std::error_code error;
  const auto sd_card_root =
      std::filesystem::weakly_canonical(sd_card_root_, error);
  if (error || !std::filesystem::is_directory(sd_card_root, error) || error) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The configured Onion SD-card root is unavailable");
  }
  const auto rom_root =
      std::filesystem::weakly_canonical(sd_card_root / contract->rom_directory,
                                        error);
  if (error || !within(rom_root, sd_card_root)) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The configured Onion ROM root cannot be resolved");
  }
  const auto rom_path = std::filesystem::weakly_canonical(target.rom_path, error);
  if (error || !within(rom_path, rom_root)) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The ROM target is outside its configured Onion system directory");
  }
  if (!std::filesystem::is_regular_file(rom_path, error) || error) {
    return result(target, LaunchOutcome::MissingRom,
                  "The selected ROM file is unavailable");
  }

  const auto extension = lowercase(rom_path.extension().string());
  if (std::find(contract->extensions.begin(), contract->extensions.end(),
                extension) == contract->extensions.end()) {
    return result(target, LaunchOutcome::UnsupportedRom,
                  "The ROM extension is not supported by the pinned Onion system");
  }

  const auto launcher =
      std::filesystem::weakly_canonical(sd_card_root / contract->launcher, error);
  if (error || !within(launcher, sd_card_root) ||
      !std::filesystem::is_regular_file(launcher, error) || error) {
    return result(target, LaunchOutcome::LauncherUnavailable,
                  "The pinned Onion system launcher is unavailable");
  }

  const auto process_result = process_.run(launcher, {rom_path.string()});
  if (!process_result.started) {
    return result(target, LaunchOutcome::ProcessStartFailed,
                  process_result.detail.empty() ? "The Onion launcher could not be started"
                                                : process_result.detail);
  }
  if (!process_result.exit_code.has_value() || *process_result.exit_code != 0) {
    return result(target, LaunchOutcome::AbnormalExit,
                  process_result.detail.empty() ? "The Onion launcher exited abnormally"
                                                : process_result.detail,
                  process_result.exit_code);
  }
  return result(target, LaunchOutcome::Completed, "The game returned normally", 0);
}

}  // namespace sprout::launcher
