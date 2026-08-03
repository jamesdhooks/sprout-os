#include "sprout/launcher/profile_archive_presentation.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using sprout::launcher::Action;
using sprout::launcher::DailyTimePolicyStore;
using sprout::launcher::NewProfile;
using sprout::launcher::ProfileArchivePresentation;
using sprout::launcher::ProfileArchivePresentationEvent;
using sprout::launcher::ProfileArchiveService;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;

void require(bool condition, std::string_view message) {
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
            ("sprout-profile-archive-ui-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

 private:
  std::filesystem::path path_;
};

NewProfile child(std::string id = "child-alex") {
  return NewProfile{
      .id = std::move(id),
      .display_name = "Alex",
      .role = ProfileRole::Child,
      .avatar_ref = "builtin:sprout",
      .save_namespace = "saves-child-alex",
      .content_policy_ref = "content:child-default",
      .time_policy_ref = "time:child-default",
  };
}

void export_is_controller_accessible_and_does_not_overwrite() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  DailyTimePolicyStore policy(directory.path() / "time.sqlite3");
  profiles.create_profile(child());
  policy.set_daily_allowance("child-alex", 2700);
  ProfileArchiveService service(profiles, policy,
                                directory.path() / "profile-images");
  ProfileArchivePresentation presentation(
      profiles, service, directory.path() / "exports",
      directory.path() / "imports");

  require(presentation.title() == "BACKUP AND RESTORE" &&
              presentation.choices().size() == 2,
          "archive flow should begin with two narrow operations");
  (void)presentation.handle(Action::Confirm);
  require(presentation.title() == "EXPORT ONE PROFILE" &&
              presentation.choices()[0] == "Alex",
          "export should list active profiles by display name");
  (void)presentation.handle(Action::Confirm);
  require(!presentation.notice_is_error() &&
              presentation.notice() == "EXPORTED Alex",
          "successful export should return a family-facing confirmation");

  std::size_t archive_count = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory.path() / "exports")) {
    if (entry.path().extension() == ".sprout-profile") {
      ++archive_count;
    }
  }
  require(archive_count == 1, "export should create exactly one archive file");

  (void)presentation.handle(Action::Confirm);
  (void)presentation.handle(Action::Confirm);
  require(presentation.notice_is_error(),
          "a second export should refuse to overwrite the first archive");
}

void local_portrait_requires_an_explicit_choice() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  DailyTimePolicyStore policy(directory.path() / "time.sqlite3");
  auto local_child = child();
  local_child.avatar_ref = "local:child-alex/portrait.png";
  profiles.create_profile(local_child);
  policy.set_daily_allowance("child-alex", 2700);
  ProfileArchiveService service(profiles, policy,
                                directory.path() / "profile-images");
  ProfileArchivePresentation presentation(
      profiles, service, directory.path() / "exports",
      directory.path() / "imports");

  (void)presentation.handle(Action::Confirm);
  (void)presentation.handle(Action::Confirm);
  require(presentation.title() == "INCLUDE PORTRAIT?" &&
              presentation.choices().size() == 2 &&
              presentation.focus_index() == 1,
          "managed portraits should pause on a cancel-first consent screen");
  (void)presentation.handle(Action::Confirm);
  require(presentation.title() == "EXPORT ONE PROFILE",
          "cancel should return without writing an archive");
  require(!std::filesystem::exists(directory.path() / "exports"),
          "cancelled portrait export must not create output");
}

void restore_lists_only_valid_archives_and_reports_profile_change() {
  TemporaryDirectory directory;
  const auto imports = directory.path() / "imports";
  std::filesystem::create_directories(imports);

  ProfileRepository source_profiles(directory.path() / "source-profiles.sqlite3");
  DailyTimePolicyStore source_policy(directory.path() / "source-time.sqlite3");
  source_profiles.create_profile(child());
  source_policy.set_daily_allowance("child-alex", 2700);
  ProfileArchiveService source_service(
      source_profiles, source_policy, directory.path() / "source-images");
  (void)source_service.export_profile(
      "child-alex", imports / "alex.sprout-profile");

  ProfileRepository target_profiles(directory.path() / "target-profiles.sqlite3");
  DailyTimePolicyStore target_policy(directory.path() / "target-time.sqlite3");
  ProfileArchiveService target_service(
      target_profiles, target_policy, directory.path() / "target-images");
  ProfileArchivePresentation presentation(
      target_profiles, target_service, directory.path() / "exports", imports);

  (void)presentation.handle(Action::Down);
  (void)presentation.handle(Action::Confirm);
  require(presentation.title() == "RESTORE ONE PROFILE" &&
              presentation.choices().size() == 1 &&
              presentation.choices()[0] == "Alex",
          "restore should list valid direct-child archives deterministically");
  const auto event = presentation.handle(Action::Confirm);
  require(event == ProfileArchivePresentationEvent::ProfilesChanged &&
              target_profiles.find_profile("child-alex").has_value(),
          "successful restore should persist the profile and request a reload");
}

}  // namespace

int main() {
  try {
    export_is_controller_accessible_and_does_not_overwrite();
    local_portrait_requires_an_explicit_choice();
    restore_lists_only_valid_archives_and_reports_profile_change();
  } catch (const std::exception& error) {
    std::cerr << "profile archive presentation test failed: " << error.what()
              << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "profile archive presentation tests passed\n";
  return EXIT_SUCCESS;
}
