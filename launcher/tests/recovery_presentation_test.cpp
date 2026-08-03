#include "sprout/launcher/recovery_presentation.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::ConfigurationStore;
using sprout::launcher::LocalConfiguration;
using sprout::launcher::RecoveryPresentation;
using sprout::launcher::RecoveryPresentationEvent;
using sprout::launcher::SetupStep;

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
            ("sprout-recovery-presentation-test-" + std::to_string(suffix));
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

void overwrite_file(const std::filesystem::path& path,
                    std::string_view contents) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << contents;
}

void restores_only_after_explicit_confirmation() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path() / "config");
  auto first = store.save(LocalConfiguration{});
  first.next_setup_step = SetupStep::Parent;
  (void)store.save(first);
  RecoveryPresentation recovery(store, 4);

  expect(recovery.choices()[0] == "RESTORE LAST-KNOWN-GOOD",
         "valid snapshot should be offered");
  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Confirm);
  expect(recovery.focus_index() == 1,
         "destructive confirmation should default to cancel");
  expect(recovery.notice().find("REVISION 1 - NEXT welcome") !=
             std::string_view::npos,
         "restore confirmation should preview revision and setup step");
  (void)recovery.handle(Action::Confirm);
  expect(store.load_active().revision == 2,
         "cancel should leave active configuration unchanged");

  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Confirm);
  (void)recovery.handle(Action::Left);
  const auto event = recovery.handle(Action::Confirm);
  expect(event == RecoveryPresentationEvent::ConfigurationChanged,
         "confirmed restore should announce configuration change");
  expect(store.load_active().revision == 1,
         "confirmed restore should activate the validated snapshot");
}

void hides_invalid_snapshot_and_exits_safely() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path() / "config");
  auto first = store.save(LocalConfiguration{});
  (void)store.save(first);
  overwrite_file(store.last_known_good_path(), "{invalid");
  RecoveryPresentation recovery(store, 2);

  expect(recovery.choices().size() == 2,
         "invalid snapshot should not expose a restore action");
  expect(recovery.notice_is_error(),
         "invalid snapshot should be visible as a recovery error");
  expect(recovery.handle(Action::Back) ==
             RecoveryPresentationEvent::ExitRequested,
         "back from recovery home should request a safe exit");
}

void resets_configuration_without_touching_other_data() {
  TemporaryDirectory directory;
  ConfigurationStore store(directory.path() / "config");
  (void)store.save(LocalConfiguration{});
  const auto sentinel = directory.path() / "data" / "profiles.sqlite3";
  std::filesystem::create_directories(sentinel.parent_path());
  overwrite_file(sentinel, "profile data");
  RecoveryPresentation recovery(store, 9);

  (void)recovery.handle(Action::Up);
  (void)recovery.handle(Action::Confirm);
  expect(recovery.focus_index() == 1,
         "reset confirmation should default to cancel");
  (void)recovery.handle(Action::Left);
  const auto event = recovery.handle(Action::Confirm);
  expect(event == RecoveryPresentationEvent::ConfigurationChanged,
         "confirmed reset should announce configuration change");
  expect(store.load_active().revision == 1 &&
             store.load_active().next_setup_step == SetupStep::Welcome,
         "reset should create fresh versioned setup configuration");
  expect(std::filesystem::exists(directory.path() / "config" / "recovery" /
                                 "sprout.failed-startup-9.json"),
         "reset should quarantine the prior active configuration");
  expect(std::filesystem::file_size(sentinel) == 12,
         "reset should preserve profile data outside configuration");
}

}  // namespace

int main() {
  try {
    restores_only_after_explicit_confirmation();
    hides_invalid_snapshot_and_exits_safely();
    resets_configuration_without_touching_other_data();
  } catch (const std::exception& error) {
    std::cerr << "recovery presentation test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "recovery presentation tests passed\n";
  return EXIT_SUCCESS;
}
