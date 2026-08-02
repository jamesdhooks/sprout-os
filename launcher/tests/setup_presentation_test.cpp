#include "sprout/launcher/setup_presentation.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::ConfigurationStore;
using sprout::launcher::ProfileRepository;
using sprout::launcher::SetupPresentation;
using sprout::launcher::SetupPresentationEvent;
using sprout::launcher::SetupStep;
using sprout::launcher::SetupWizard;

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
            ("sprout-setup-presentation-test-" + std::to_string(suffix));
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

struct SetupFixture {
  TemporaryDirectory directory;
  ConfigurationStore configuration{directory.path() / "config"};
  ProfileRepository profiles{directory.path() / "profiles.sqlite3"};
  SetupWizard wizard{configuration, profiles};
  SetupPresentation presentation{wizard};
};

void renders_content_for_each_persisted_step() {
  SetupFixture fixture;
  expect(fixture.presentation.step() == SetupStep::Welcome,
         "presentation should begin at persisted welcome step");
  expect(fixture.presentation.title() == "WELCOME TO SPROUT",
         "welcome should provide a concrete title");

  (void)fixture.presentation.handle(Action::Confirm);
  expect(fixture.presentation.step() == SetupStep::Locale,
         "welcome confirmation should advance to locale");
  expect(fixture.presentation.choices().size() == 2,
         "locale should expose two deterministic offline choices");

  (void)fixture.presentation.handle(Action::Left);
  expect(fixture.presentation.focus_index() == 1,
         "left should wrap across setup choices");
  (void)fixture.presentation.handle(Action::Right);
  expect(fixture.presentation.focus_index() == 0,
         "right should return to the first setup choice");
}

void completes_the_desktop_setup_path() {
  SetupFixture fixture;
  (void)fixture.presentation.handle(Action::Confirm);  // Welcome
  (void)fixture.presentation.handle(Action::Confirm);  // Canada locale
  (void)fixture.presentation.handle(Action::Confirm);  // Offline
  (void)fixture.presentation.handle(Action::Right);    // Owl parent
  (void)fixture.presentation.handle(Action::Confirm);  // Parent
  (void)fixture.presentation.handle(Action::Confirm);  // Skip PIN
  (void)fixture.presentation.handle(Action::Confirm);  // Add child
  (void)fixture.presentation.handle(Action::Confirm);  // Avatars
  (void)fixture.presentation.handle(Action::Confirm);  // Library
  (void)fixture.presentation.handle(Action::Confirm);  // Defaults
  (void)fixture.presentation.handle(Action::Confirm);  // Connectors
  const auto completed = fixture.presentation.handle(Action::Confirm);  // Review

  expect(completed == SetupPresentationEvent::Completed,
         "review confirmation should announce setup completion");
  expect(fixture.presentation.step() == SetupStep::Complete,
         "desktop path should persist complete setup");
  expect(fixture.profiles.list_profiles().size() == 2,
         "desktop path should create parent and child profiles");
  expect(fixture.profiles.find_profile("parent-primary")->avatar_ref ==
             "builtin:owl",
         "parent portrait choice should reach profile persistence");
}

void back_requests_exit_without_losing_progress() {
  SetupFixture fixture;
  (void)fixture.presentation.handle(Action::Confirm);
  const auto exited = fixture.presentation.handle(Action::Back);
  expect(exited == SetupPresentationEvent::ExitRequested,
         "back should leave setup through an explicit event");

  SetupWizard resumed_wizard(fixture.configuration, fixture.profiles);
  expect(resumed_wizard.current_step() == SetupStep::Locale,
         "leaving should preserve the last completed setup step");
}

void requests_and_completes_available_parent_image_import() {
  TemporaryDirectory directory;
  ConfigurationStore configuration(directory.path() / "config");
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  SetupWizard wizard(configuration, profiles);
  SetupPresentation presentation(wizard, true);
  for (int index = 0; index < 6; ++index) {
    (void)presentation.handle(Action::Confirm);
  }
  expect(presentation.step() == SetupStep::Avatars,
         "custom image fixture should reach portrait setup");
  expect(presentation.choices().size() == 2,
         "available import should retain an explicit built-in fallback");
  expect(presentation.handle(Action::Confirm) ==
             SetupPresentationEvent::ImportParentImageRequested,
         "first portrait choice should request the crop flow");
  expect(presentation.step() == SetupStep::Avatars,
         "requesting an import should not advance before activation");
  presentation.complete_avatar_step();
  expect(presentation.step() == SetupStep::Library,
         "successful import should resume at the next setup step");
}

}  // namespace

int main() {
  try {
    renders_content_for_each_persisted_step();
    completes_the_desktop_setup_path();
    back_requests_exit_without_losing_progress();
    requests_and_completes_available_parent_image_import();
  } catch (const std::exception& error) {
    std::cerr << "setup presentation test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "setup presentation tests passed\n";
  return EXIT_SUCCESS;
}
