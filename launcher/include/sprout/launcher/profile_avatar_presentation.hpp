#pragma once

#include "sprout/launcher/built_in_avatar.hpp"
#include "sprout/launcher/built_in_background.hpp"
#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_repository.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

enum class ProfileAvatarStage {
  Profile,
  Appearance,
  Avatar,
  Background,
};

enum class ProfileAvatarEventType {
  BackRequested,
  AvatarAssigned,
  BackgroundAssigned,
  ImportRequested,
};

struct ProfileAvatarEvent {
  ProfileAvatarEventType type;
  std::string profile_id;
};

class ProfileAvatarPresentation {
 public:
  explicit ProfileAvatarPresentation(
      ProfileRepository& profiles, bool custom_image_available,
      std::optional<std::string> initial_profile_id = std::nullopt,
      ProfileAvatarStage initial_stage = ProfileAvatarStage::Avatar);

  [[nodiscard]] ProfileAvatarStage stage() const noexcept;
  [[nodiscard]] ReadOnlyView<ProfileRecord> profiles() const noexcept;
  [[nodiscard]] const ProfileRecord* selected_profile() const noexcept;
  [[nodiscard]] ReadOnlyView<BuiltInAvatar> avatars() const noexcept;
  [[nodiscard]] ReadOnlyView<BuiltInBackground> backgrounds() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::size_t page_index() const noexcept;
  [[nodiscard]] std::size_t page_count() const noexcept;
  [[nodiscard]] bool custom_image_available() const noexcept;
  [[nodiscard]] bool import_focused() const noexcept;
  [[nodiscard]] const std::string& notice() const noexcept;

  [[nodiscard]] std::optional<ProfileAvatarEvent> handle(Action action);

 private:
  void move_profile_focus(int delta);
  void move_avatar_focus(int delta);
  void move_background_focus(int delta);

  ProfileRepository& repository_;
  std::vector<ProfileRecord> profiles_;
  bool custom_image_available_{false};
  bool profile_locked_{false};
  ProfileAvatarStage stage_{ProfileAvatarStage::Profile};
  std::size_t profile_focus_{0};
  std::size_t avatar_focus_{0};
  std::size_t appearance_focus_{0};
  std::size_t background_focus_{0};
  std::string notice_;
};

}  // namespace sprout::launcher
