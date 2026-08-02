#include "sprout/launcher/setup_wizard.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::ConfigurationStore;
using sprout::launcher::LocaleOverrides;
using sprout::launcher::NewProfile;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;
using sprout::launcher::SetupStep;
using sprout::launcher::SetupWizard;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Operation>
void expect_failure(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-setup-test-" + std::to_string(suffix));
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

NewProfile parent() {
  return NewProfile{
      .id = "parent-sam",
      .display_name = "Sam",
      .role = ProfileRole::Parent,
      .avatar_ref = "builtin:fox",
      .save_namespace = "saves-parent-sam",
  };
}

NewProfile child() {
  return NewProfile{
      .id = "child-alex",
      .display_name = "Alex",
      .role = ProfileRole::Child,
      .avatar_ref = "builtin:sprout",
      .save_namespace = "saves-child-alex",
      .content_policy_ref = "content:child-default",
      .time_policy_ref = "time:child-default",
  };
}

template <typename Operation>
void resume_at(const std::filesystem::path& configuration_path,
               ProfileRepository& profiles, SetupStep expected,
               Operation operation) {
  ConfigurationStore store(configuration_path);
  SetupWizard wizard(store, profiles);
  expect(wizard.current_step() == expected,
         "wizard should resume at the last persisted step");
  operation(wizard);
}

void completes_parent_and_child_setup_offline_across_restarts() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  const auto configuration_path = directory.path() / "config";

  resume_at(configuration_path, profiles, SetupStep::Welcome,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  resume_at(configuration_path, profiles, SetupStep::Locale,
            [](SetupWizard& wizard) {
              wizard.configure_locale(LocaleOverrides{
                  .language = "en",
                  .region = "CA",
                  .time_zone = "America/Toronto",
              });
            });
  resume_at(configuration_path, profiles, SetupStep::Network,
            [](SetupWizard& wizard) { wizard.continue_offline(); });
  resume_at(configuration_path, profiles, SetupStep::Parent,
            [](SetupWizard& wizard) { wizard.create_parent(parent()); });
  resume_at(configuration_path, profiles, SetupStep::ParentPin,
            [](SetupWizard& wizard) {
              wizard.set_parent_credential_ref("secret:parent-pin");
            });
  resume_at(configuration_path, profiles, SetupStep::Child,
            [](SetupWizard& wizard) { wizard.create_child(child()); });
  resume_at(configuration_path, profiles, SetupStep::Avatars,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  resume_at(configuration_path, profiles, SetupStep::Library,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  resume_at(configuration_path, profiles, SetupStep::ChildDefaults,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  resume_at(configuration_path, profiles, SetupStep::Connectors,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  resume_at(configuration_path, profiles, SetupStep::Review,
            [](SetupWizard& wizard) { wizard.finish(); });

  resume_at(configuration_path, profiles, SetupStep::Complete,
            [](SetupWizard& wizard) {
              expect(wizard.configuration().offline_setup,
                     "completed setup should remain local and offline");
              expect(wizard.configuration().parent_credential_ref ==
                         "secret:parent-pin",
                     "configuration should retain only the credential reference");
            });
  expect(profiles.list_profiles().size() == 2,
         "completed setup should persist one parent and one child");
}

void allows_every_optional_step_to_be_skipped() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  const auto configuration_path = directory.path() / "config";

  ConfigurationStore store(configuration_path);
  SetupWizard wizard(store, profiles);
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();
  expect_failure([&] { wizard.skip_current_step(); },
                 "first parent creation should be the only unskippable step");
  wizard.create_parent(parent());
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.finish();
  expect(wizard.current_step() == SetupStep::Complete,
         "setup should complete with all optional steps skipped");
  expect(!wizard.configuration().parent_credential_ref.has_value(),
         "skipping PIN should not create placeholder credential data");
}

void resumes_after_profile_write_before_step_write() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  const auto configuration_path = directory.path() / "config";
  {
    ConfigurationStore store(configuration_path);
    SetupWizard wizard(store, profiles);
    wizard.skip_current_step();
    wizard.skip_current_step();
    wizard.skip_current_step();
  }

  profiles.create_profile(parent());
  resume_at(configuration_path, profiles, SetupStep::Parent,
            [](SetupWizard& wizard) { wizard.create_parent(parent()); });
  expect(profiles.list_profiles().size() == 1,
         "idempotent resume should not duplicate the parent profile");

  resume_at(configuration_path, profiles, SetupStep::ParentPin,
            [](SetupWizard& wizard) { wizard.skip_current_step(); });
  profiles.create_profile(child());
  resume_at(configuration_path, profiles, SetupStep::Child,
            [](SetupWizard& wizard) { wizard.create_child(child()); });
  expect(profiles.list_profiles().size() == 2,
         "idempotent resume should not duplicate the child profile");
}

void rejects_out_of_order_and_conflicting_profile_data() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  ConfigurationStore store(directory.path() / "config");
  SetupWizard wizard(store, profiles);
  expect_failure([&] { wizard.create_parent(parent()); },
                 "wizard should reject operations for a different step");
  wizard.skip_current_step();
  wizard.skip_current_step();
  wizard.skip_current_step();

  auto conflicting = parent();
  conflicting.display_name = "Different";
  profiles.create_profile(conflicting);
  expect_failure([&] { wizard.create_parent(parent()); },
                 "resume should reject a conflicting existing profile ID");
  expect(wizard.current_step() == SetupStep::Parent,
         "conflicting profile should not advance setup");
}

}  // namespace

int main() {
  try {
    completes_parent_and_child_setup_offline_across_restarts();
    allows_every_optional_step_to_be_skipped();
    resumes_after_profile_write_before_step_write();
    rejects_out_of_order_and_conflicting_profile_data();
  } catch (const std::exception& error) {
    std::cerr << "setup wizard test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "setup wizard tests passed\n";
  return EXIT_SUCCESS;
}
