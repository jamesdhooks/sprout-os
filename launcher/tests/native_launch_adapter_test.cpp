#include "sprout/launcher/native_launch_adapter.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using sprout::launcher::LaunchProcess;
using sprout::launcher::NativeLaunchAdapter;
using sprout::launcher::NativeLaunchMode;
using sprout::launcher::NativeLaunchOutcome;
using sprout::launcher::NativeLaunchTarget;
using sprout::launcher::ProcessResult;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

class Fixture {
 public:
  Fixture() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("sprout-native-launch-" + std::to_string(nonce));
    package_ = root_ / "package";
    runtime_ = root_ / "bin" / "sprout-runtime.exe";
    std::filesystem::create_directories(package_);
    std::filesystem::create_directories(runtime_.parent_path());
    std::ofstream(package_ / "manifest.json") << R"({
  "schemaVersion": 1,
  "id": "sprout.snake",
  "title": "Sprout Snake",
  "version": "1.0.0",
  "runtimeVersion": 1,
  "entrypoint": "game.lua",
  "logicalResolution": [320, 240],
  "audience": "family",
  "capabilities": []
})";
    std::ofstream(package_ / "game.lua")
        << "function init() end\nfunction update(a) end\n"
           "function render() end\nfunction snapshot() return '' end\n";
    std::ofstream(runtime_) << "fixture";
  }

  ~Fixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }

  const std::filesystem::path& root() const { return root_; }
  const std::filesystem::path& package() const { return package_; }
  const std::filesystem::path& runtime() const { return runtime_; }

 private:
  std::filesystem::path root_;
  std::filesystem::path package_;
  std::filesystem::path runtime_;
};

class RecordingProcess final : public LaunchProcess {
 public:
  ProcessResult next{.started = true, .exit_code = 0, .detail = {}};
  int calls{};
  std::filesystem::path executable;
  std::vector<std::string> arguments;

  ProcessResult run(const std::filesystem::path& requested_executable,
                    const std::vector<std::string>& requested_arguments) override {
    ++calls;
    executable = requested_executable;
    arguments = requested_arguments;
    return next;
  }
};

NativeLaunchTarget allowed(const Fixture& fixture) {
  return {
      .item_id = "arcade:sprout.snake",
      .package_root = std::filesystem::canonical(fixture.package()),
      .profile_id = "child-alex",
      .seed = 7,
      .launch_allowed = true,
  };
}

void valid_launch_uses_fixed_runtime_and_profile_storage() {
  Fixture fixture;
  RecordingProcess process;
  NativeLaunchAdapter adapter(fixture.runtime(), fixture.root() / "storage",
                              process);
  const auto result = adapter.launch(allowed(fixture));
  require(result.completed() && process.calls == 1,
          "valid native target should complete through one process");
  require(process.executable == std::filesystem::canonical(fixture.runtime()),
          "native launch should use only the configured runtime executable");
  require(process.arguments.size() == 6 &&
              process.arguments[0] == "--package" &&
              process.arguments[2] == "--storage" &&
              process.arguments[4] == "--seed" &&
              process.arguments[5] == "7",
          "native launch should use structured runtime arguments");
  require(std::filesystem::is_directory(fixture.root() / "storage" /
                                        "child-alex"),
          "native storage should be isolated by active profile");
}

void smoke_launch_requests_a_noninteractive_runtime_session() {
  Fixture fixture;
  RecordingProcess process;
  NativeLaunchAdapter adapter(fixture.runtime(), fixture.root() / "storage",
                              process);
  const auto result = adapter.launch(allowed(fixture), NativeLaunchMode::SmokeTest);
  require(result.completed() && process.arguments.size() == 7 &&
              process.arguments.back() == "--smoke-test",
          "smoke launch should use the normal target with a noninteractive runtime");
}

void rejected_targets_never_start_a_process() {
  Fixture fixture;
  RecordingProcess process;
  NativeLaunchAdapter adapter(fixture.runtime(), fixture.root() / "storage",
                              process);
  auto target = allowed(fixture);
  target.launch_allowed = false;
  require(adapter.launch(target).outcome == NativeLaunchOutcome::PolicyDenied,
          "policy denial should be explicit");
  target = allowed(fixture);
  target.item_id = "arcade:sprout.changed";
  require(adapter.launch(target).outcome == NativeLaunchOutcome::InvalidTarget,
          "identity mismatch should fail closed");
  target = allowed(fixture);
  target.profile_id = "../escape";
  require(adapter.launch(target).outcome == NativeLaunchOutcome::InvalidTarget,
          "profile traversal should fail closed");
  require(process.calls == 0, "rejected targets must not start a process");
}

void runtime_and_process_failures_are_structured() {
  Fixture fixture;
  RecordingProcess process;
  NativeLaunchAdapter missing(fixture.root() / "missing-runtime.exe",
                              fixture.root() / "storage", process);
  require(missing.launch(allowed(fixture)).outcome ==
              NativeLaunchOutcome::RuntimeUnavailable,
          "missing runtime should be reported");

  NativeLaunchAdapter adapter(fixture.runtime(), fixture.root() / "storage",
                              process);
  process.next = {.started = false,
                  .exit_code = std::nullopt,
                  .detail = "start failed"};
  require(adapter.launch(allowed(fixture)).outcome ==
              NativeLaunchOutcome::ProcessStartFailed,
          "process start failure should be retained");
  process.next = {.started = true, .exit_code = 9, .detail = {}};
  const auto abnormal = adapter.launch(allowed(fixture));
  require(abnormal.outcome == NativeLaunchOutcome::AbnormalExit &&
              abnormal.exit_code == 9,
          "abnormal runtime exit should retain its status");
}

}  // namespace

int main() {
  try {
    valid_launch_uses_fixed_runtime_and_profile_storage();
    smoke_launch_requests_a_noninteractive_runtime_session();
    rejected_targets_never_start_a_process();
    runtime_and_process_failures_are_structured();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
