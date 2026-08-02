#include "sprout/launcher/onion_launch_adapter.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using sprout::launcher::EmulatedLaunchTarget;
using sprout::launcher::LaunchOutcome;
using sprout::launcher::LaunchProcess;
using sprout::launcher::OnionLaunchAdapter;
using sprout::launcher::OnionSystem;
using sprout::launcher::ProcessResult;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class TemporaryCard {
 public:
  TemporaryCard() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("sprout-onion-adapter-" + std::to_string(nonce));
    std::filesystem::create_directories(root_ / "Roms/GB");
    std::filesystem::create_directories(root_ / "Roms/SFC");
    std::filesystem::create_directories(root_ / "Emu/GB");
    std::filesystem::create_directories(root_ / "Emu/SFC");
    touch(root_ / "Emu/GB/launch.sh");
    touch(root_ / "Emu/SFC/launch.sh");
  }

  ~TemporaryCard() { std::filesystem::remove_all(root_); }

  TemporaryCard(const TemporaryCard&) = delete;
  TemporaryCard& operator=(const TemporaryCard&) = delete;

  [[nodiscard]] const std::filesystem::path& root() const { return root_; }

  std::filesystem::path rom(std::string_view relative_path) {
    const auto path = root_ / "Roms" / relative_path;
    std::filesystem::create_directories(path.parent_path());
    touch(path);
    return path;
  }

 private:
  static void touch(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    output << "test fixture";
  }

  std::filesystem::path root_;
};

class RecordingProcess final : public LaunchProcess {
 public:
  ProcessResult next{.started = true, .exit_code = 0, .detail = {}};
  int calls{0};
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

EmulatedLaunchTarget allowed(OnionSystem system,
                             const std::filesystem::path& rom_path) {
  return {
      .item_id = "game-1",
      .system = system,
      .rom_path = rom_path,
      .launch_allowed = true,
  };
}

void test_gb_and_snes_contracts() {
  TemporaryCard card;
  RecordingProcess process;
  OnionLaunchAdapter adapter(card.root(), process);

  const auto gb_rom = card.rom("GB/Family Game.GB");
  auto result = adapter.launch(allowed(OnionSystem::GameBoy, gb_rom));
  require(result.completed(), "GB launch should complete");
  require(process.executable == std::filesystem::canonical(card.root() / "Emu/GB/launch.sh"),
          "GB launch should use Onion's GB script");
  require(process.arguments == std::vector<std::string>{std::filesystem::canonical(gb_rom).string()},
          "ROM path should remain one process argument");

  const auto snes_rom = card.rom("SFC/Family Game.sfc");
  result = adapter.launch(allowed(OnionSystem::SuperNintendo, snes_rom));
  require(result.completed(), "SNES launch should complete");
  require(process.executable == std::filesystem::canonical(card.root() / "Emu/SFC/launch.sh"),
          "SNES launch should use Onion's SFC script");
  require(process.arguments == std::vector<std::string>{std::filesystem::canonical(snes_rom).string()},
          "SNES ROM path should remain one process argument");
}

void test_rejections_do_not_start_process() {
  TemporaryCard card;
  RecordingProcess process;
  OnionLaunchAdapter adapter(card.root(), process);
  const auto rom = card.rom("GB/allowed.gb");

  auto denied = allowed(OnionSystem::GameBoy, rom);
  denied.launch_allowed = false;
  require(adapter.launch(denied).outcome == LaunchOutcome::PolicyDenied,
          "disallowed item should be denied");

  auto missing = allowed(OnionSystem::GameBoy, card.root() / "Roms/GB/missing.gb");
  require(adapter.launch(missing).outcome == LaunchOutcome::MissingRom,
          "missing ROM should be reported");

  const auto wrong_extension = card.rom("GB/readme.txt");
  require(adapter.launch(allowed(OnionSystem::GameBoy, wrong_extension)).outcome ==
              LaunchOutcome::UnsupportedRom,
          "unsupported extension should be rejected");

  const auto outside = card.root().parent_path() / "outside.gb";
  {
    std::ofstream output(outside);
    output << "outside";
  }
  require(adapter.launch(allowed(OnionSystem::GameBoy, outside)).outcome ==
              LaunchOutcome::InvalidTarget,
          "ROM outside the system root should be rejected");
  std::filesystem::remove(outside);

  auto relative = allowed(OnionSystem::GameBoy, "Roms/GB/allowed.gb");
  require(adapter.launch(relative).outcome == LaunchOutcome::InvalidTarget,
          "relative ROM target should be rejected");

  auto unsupported_system =
      allowed(static_cast<OnionSystem>(99), rom);
  require(adapter.launch(unsupported_system).outcome == LaunchOutcome::InvalidTarget,
          "unknown Onion system should be rejected");

  auto missing_identity = allowed(OnionSystem::GameBoy, rom);
  missing_identity.item_id.clear();
  require(adapter.launch(missing_identity).outcome == LaunchOutcome::InvalidTarget,
          "target without an item identity should be rejected");
  require(process.calls == 0, "rejected targets must not start a process");
}

void test_launcher_and_process_outcomes() {
  TemporaryCard card;
  RecordingProcess process;
  OnionLaunchAdapter adapter(card.root(), process);
  const auto rom = card.rom("GB/game.zip");

  std::filesystem::remove(card.root() / "Emu/GB/launch.sh");
  require(adapter.launch(allowed(OnionSystem::GameBoy, rom)).outcome ==
              LaunchOutcome::LauncherUnavailable,
          "missing Onion launcher should be reported");
  require(process.calls == 0, "missing launcher must not start a process");

  {
    std::ofstream output(card.root() / "Emu/GB/launch.sh");
    output << "fixture";
  }
  process.next = {
      .started = false,
      .exit_code = std::nullopt,
      .detail = "permission denied",
  };
  auto result = adapter.launch(allowed(OnionSystem::GameBoy, rom));
  require(result.outcome == LaunchOutcome::ProcessStartFailed,
          "process start failure should be structured");

  process.next = {
      .started = true,
      .exit_code = 23,
      .detail = "core failed",
  };
  result = adapter.launch(allowed(OnionSystem::GameBoy, rom));
  require(result.outcome == LaunchOutcome::AbnormalExit && result.exit_code == 23,
          "non-zero exit should retain its status");

  process.next = {
      .started = true,
      .exit_code = std::nullopt,
      .detail = "terminated",
  };
  result = adapter.launch(allowed(OnionSystem::GameBoy, rom));
  require(result.outcome == LaunchOutcome::AbnormalExit && !result.exit_code,
          "missing exit status should be abnormal");
}

}  // namespace

int main() {
  try {
    test_gb_and_snes_contracts();
    test_rejections_do_not_start_process();
    test_launcher_and_process_outcomes();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
