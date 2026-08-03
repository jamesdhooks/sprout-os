#include "sprout/launcher/profile_avatar_presentation.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::NewProfile;
using sprout::launcher::ProfileAvatarEventType;
using sprout::launcher::ProfileAvatarPresentation;
using sprout::launcher::ProfileAvatarStage;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;

void expect(bool condition, std::string_view message) {
  if (!condition) throw std::runtime_error(std::string(message));
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-profile-avatar-test-" + std::to_string(suffix));
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

void add_profiles(ProfileRepository& profiles) {
  profiles.create_profile(NewProfile{
      .id = "parent-primary",
      .display_name = "Parent",
      .role = ProfileRole::Parent,
      .avatar_ref = "builtin:rocket-ship",
      .save_namespace = "saves-parent-primary",
  });
  profiles.create_profile(NewProfile{
      .id = "child-primary",
      .display_name = "Alex",
      .role = ProfileRole::Child,
      .avatar_ref = "builtin:sunflower",
      .save_namespace = "saves-child-primary",
      .content_policy_ref = "content:child-default",
      .time_policy_ref = "time:child-default",
  });
}

void assigns_a_catalogue_avatar_to_a_selected_profile() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  add_profiles(profiles);
  ProfileAvatarPresentation presentation(profiles, false);
  expect(presentation.stage() == ProfileAvatarStage::Profile,
         "settings should begin with profile selection");
  (void)presentation.handle(Action::Confirm);
  expect(presentation.stage() == ProfileAvatarStage::Avatar &&
             presentation.avatars().size() == 32,
         "profile selection should expose all 32 built-in avatars");
  (void)presentation.handle(Action::Right);
  const auto assigned = presentation.handle(Action::Confirm);
  expect(assigned.has_value() &&
             assigned->type == ProfileAvatarEventType::AvatarAssigned,
         "confirming a built-in avatar should report an assignment");
  expect(profiles.find_profile("parent-primary")->avatar_ref ==
             "builtin:sunflower",
         "assignment should persist the selected built-in reference");
}

void exposes_custom_import_as_a_final_paged_choice() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  add_profiles(profiles);
  ProfileAvatarPresentation presentation(profiles, true, "child-primary");
  expect(presentation.page_count() == 5,
         "32 built-ins plus custom import should span five pages");
  for (int index = 0; index < 8; ++index) {
    (void)presentation.handle(Action::Down);
  }
  expect(presentation.import_focused(),
         "grid navigation should reach the custom import choice");
  const auto import = presentation.handle(Action::Confirm);
  expect(import.has_value() &&
             import->type == ProfileAvatarEventType::ImportRequested &&
             import->profile_id == "child-primary",
         "custom import should retain the selected profile identity");
}

}  // namespace

int main() {
  try {
    assigns_a_catalogue_avatar_to_a_selected_profile();
    exposes_custom_import_as_a_final_paged_choice();
  } catch (const std::exception& error) {
    std::cerr << "profile avatar presentation test failed: " << error.what()
              << '\n';
    return 1;
  }
  std::cout << "profile avatar presentation tests passed\n";
  return 0;
}
