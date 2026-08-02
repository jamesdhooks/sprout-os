#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class ProfileRole {
  Child,
  Parent,
};

struct Profile {
  std::string id;
  std::string display_name;
  ProfileRole role;
  std::uint32_t accent_rgb;
  std::string avatar_ref;
};

enum class Screen {
  ProfileSelect,
  ChildHome,
  ParentHome,
};

enum class Action {
  Up,
  Down,
  Left,
  Right,
  Confirm,
  Back,
  ZoomIn,
  ZoomOut,
};

enum class EventType {
  ProfileActivated,
  ReturnedToProfiles,
  MenuItemInvoked,
  ExitRequested,
};

struct LauncherEvent {
  EventType type;
  std::string profile_id;
  std::string target;
};

std::vector<Profile> make_demo_household();

class LauncherState {
 public:
  explicit LauncherState(std::vector<Profile> profiles);

  [[nodiscard]] Screen screen() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::span<const Profile> profiles() const noexcept;
  [[nodiscard]] const Profile* active_profile() const noexcept;
  [[nodiscard]] std::span<const std::string_view> menu_items() const noexcept;
  [[nodiscard]] std::optional<LauncherEvent> handle(Action action);

 private:
  void move_focus(int delta, std::size_t item_count);

  std::vector<Profile> profiles_;
  Screen screen_{Screen::ProfileSelect};
  std::size_t profile_focus_{0};
  std::size_t menu_focus_{0};
  std::optional<std::size_t> active_profile_index_;
};

}  // namespace sprout::launcher
