#include "sprout/launcher/onion_runtime_handoff.hpp"

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>

namespace sprout::launcher {
namespace {

std::string shell_quote(std::string_view value) {
  std::string quoted{"'"};
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += '\'';
  return quoted;
}

ProcessResult failed(std::string detail) {
  return {.started = false, .exit_code = std::nullopt, .detail = std::move(detail)};
}

}  // namespace

OnionRuntimeHandoffProcess::OnionRuntimeHandoffProcess(
    std::filesystem::path runtime_root)
    : runtime_root_(std::move(runtime_root)) {}

ProcessResult OnionRuntimeHandoffProcess::run(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) {
  if (arguments.size() != 1 || executable.empty() || arguments[0].empty()) {
    return failed("Onion runtime handoff requires one validated ROM argument");
  }

  std::error_code error;
  const auto runtime_root =
      std::filesystem::weakly_canonical(runtime_root_, error);
  if (error || !std::filesystem::is_directory(runtime_root, error) || error) {
    return failed("The Onion runtime handoff directory is unavailable");
  }

  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto staged_command =
      runtime_root / (".sprout-command-" + std::to_string(nonce));
  const auto command = runtime_root / "cmd_to_run.sh";
  const auto marker = runtime_root / ".sprout-handoff";

  if (std::filesystem::exists(marker, error) || error) {
    return failed("A previous Onion runtime handoff is still pending");
  }

  {
    std::ofstream output(staged_command, std::ios::binary | std::ios::trunc);
    if (!output) {
      return failed("The staged Onion command could not be created");
    }
    output << "#!/bin/sh\n"
           << "LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so exec "
           << shell_quote(executable.string()) << ' '
           << shell_quote(arguments[0]) << "\n";
    output.flush();
    if (!output) {
      std::filesystem::remove(staged_command, error);
      return failed("The staged Onion command could not be written");
    }
  }

  std::filesystem::permissions(
      staged_command,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec |
          std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::replace, error);
  if (error) {
    std::filesystem::remove(staged_command, error);
    return failed("The staged Onion command could not be made executable");
  }

  std::filesystem::remove(command, error);
  error.clear();
  std::filesystem::rename(staged_command, command, error);
  if (error) {
    std::filesystem::remove(staged_command, error);
    return failed("The Onion command could not be activated atomically");
  }

  {
    std::ofstream output(marker, std::ios::binary | std::ios::trunc);
    if (!output) {
      std::filesystem::remove(command, error);
      return failed("The Onion handoff marker could not be created");
    }
    output << "sprout-handoff-v1\n";
    output.flush();
    if (!output) {
      std::filesystem::remove(marker, error);
      std::filesystem::remove(command, error);
      return failed("The Onion handoff marker could not be committed");
    }
  }

  return {
      .started = true,
      .exit_code = 0,
      .detail = "Validated game request handed to Onion runtime",
  };
}

}  // namespace sprout::launcher
