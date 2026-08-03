#include "sprout/launcher/launch_process.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#endif

namespace sprout::launcher {
namespace {

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         value.data(),
                                         static_cast<int>(value.size()),
                                         nullptr, 0);
  if (length <= 0) {
    throw std::runtime_error("Launch argument is not valid UTF-8");
  }
  std::wstring converted(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), converted.data(),
                          length) != length) {
    throw std::runtime_error("Launch argument could not be converted to UTF-16");
  }
  return converted;
}
#endif

}  // namespace

ProcessResult SystemLaunchProcess::run(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) {
#ifdef _WIN32
  try {
    std::vector<std::wstring> owned_arguments;
    owned_arguments.reserve(arguments.size() + 1);
    owned_arguments.push_back(executable.native());
    for (const auto& argument : arguments) {
      owned_arguments.push_back(utf8_to_wide(argument));
    }
    std::vector<const wchar_t*> pointers;
    pointers.reserve(owned_arguments.size() + 1);
    for (const auto& argument : owned_arguments) {
      pointers.push_back(argument.c_str());
    }
    pointers.push_back(nullptr);

    errno = 0;
    const auto status = _wspawnv(_P_WAIT, executable.c_str(), pointers.data());
    if (status == -1) {
      return {
          .started = false,
          .exit_code = std::nullopt,
          .detail = std::error_code(errno, std::generic_category()).message(),
      };
    }
    return {
        .started = true,
        .exit_code = static_cast<int>(status),
        .detail = {},
    };
  } catch (const std::exception& error) {
    return {
        .started = false,
        .exit_code = std::nullopt,
        .detail = error.what(),
    };
  }
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
        .detail = "Launcher process terminated by a signal",
    };
  }
  return {
      .started = true,
      .exit_code = std::nullopt,
      .detail = "Launcher process ended without an exit status",
  };
#endif
}

}  // namespace sprout::launcher
