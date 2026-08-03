#include "sprout/launcher/launcher_state.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::array<std::string_view, 6> kChildMenu{
    "Continue",
    "Favorites",
    "See All",
    "Sprout Arcade",
    "Ask for More Time",
    "Profile Select",
};

constexpr std::array<std::string_view, 10> kParentMenu{
    "Continue",
    "Favorites",
    "All Games",
    "Sprout Arcade",
    "Family Dashboard",
    "Profile Settings",
    "Onion Tools",
    "Backup & Restore",
    "Lock Parent Access",
    "Profile Select",
};

}  // namespace

LauncherState::LauncherState(std::vector<Profile> profiles)
    : profiles_(std::move(profiles)) {
  if (profiles_.empty()) {
    throw std::invalid_argument("A launcher household requires at least one profile");
  }
}

Screen LauncherState::screen() const noexcept { return screen_; }

std::size_t LauncherState::focus_index() const noexcept {
  return screen_ == Screen::ProfileSelect ? profile_focus_ : menu_focus_;
}

ReadOnlyView<Profile> LauncherState::profiles() const noexcept {
  return profiles_;
}

const Profile* LauncherState::active_profile() const noexcept {
  if (!active_profile_index_.has_value()) {
    return nullptr;
  }
  return &profiles_[*active_profile_index_];
}

ReadOnlyView<std::string_view> LauncherState::menu_items() const noexcept {
  if (screen_ == Screen::ChildHome) {
    return kChildMenu;
  }
  if (screen_ == Screen::ParentHome) {
    return kParentMenu;
  }
  return {};
}

std::optional<LauncherEvent> LauncherState::handle(Action action) {
  if (screen_ == Screen::ProfileSelect) {
    if (action == Action::Left || action == Action::Up) {
      move_focus(-1, profiles_.size());
      return std::nullopt;
    }
    if (action == Action::Right || action == Action::Down) {
      move_focus(1, profiles_.size());
      return std::nullopt;
    }
    if (action == Action::Back) {
      return LauncherEvent{
          .type = EventType::ExitRequested,
          .profile_id = {},
          .target = {},
      };
    }
    if (action != Action::Confirm) {
      return std::nullopt;
    }

    active_profile_index_ = profile_focus_;
    menu_focus_ = 0;
    const auto& profile = profiles_[profile_focus_];
    screen_ = profile.role == ProfileRole::Child ? Screen::ChildHome
                                                 : Screen::ParentHome;
    return LauncherEvent{
        .type = EventType::ProfileActivated,
        .profile_id = profile.id,
        .target = {},
    };
  }

  const auto items = menu_items();
  if (action == Action::Up || action == Action::Left) {
    move_focus(-1, items.size());
    return std::nullopt;
  }
  if (action == Action::Down || action == Action::Right) {
    move_focus(1, items.size());
    return std::nullopt;
  }
  if (action == Action::Back) {
    const std::string profile_id = active_profile()->id;
    screen_ = Screen::ProfileSelect;
    profile_focus_ = *active_profile_index_;
    active_profile_index_.reset();
    return LauncherEvent{
        .type = EventType::ReturnedToProfiles,
        .profile_id = profile_id,
        .target = {},
    };
  }
  if (action != Action::Confirm) {
    return std::nullopt;
  }

  const auto selected = items[menu_focus_];
  if (selected == "Profile Select") {
    return handle(Action::Back);
  }

  return LauncherEvent{
      .type = EventType::MenuItemInvoked,
      .profile_id = active_profile()->id,
      .target = std::string(selected),
  };
}

void LauncherState::move_focus(int delta, std::size_t item_count) {
  if (item_count == 0) {
    return;
  }

  auto& focus = screen_ == Screen::ProfileSelect ? profile_focus_ : menu_focus_;
  const auto signed_count = static_cast<long long>(item_count);
  const auto signed_focus = static_cast<long long>(focus);
  focus = static_cast<std::size_t>(
      (signed_focus + static_cast<long long>(delta) + signed_count) % signed_count);
}

}  // namespace sprout::launcher
