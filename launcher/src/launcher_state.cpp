#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_select_layout.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::array<std::string_view, 4> kChildMenu{
    "Game Dashboard",
    "Profile Picture",
    "Background",
    "Profile Select",
};

constexpr std::array<std::string_view, 5> kParentMenu{
    "Game Guide",
    "Profile Picture",
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
    if (action == Action::Left) {
      move_focus(-1, profiles_.size());
      return std::nullopt;
    }
    if (action == Action::Right) {
      move_focus(1, profiles_.size());
      return std::nullopt;
    }
    if (action == Action::Up || action == Action::Down) {
      move_profile_focus_vertical(action == Action::Up ? -1 : 1);
      return std::nullopt;
    }
    if (action == Action::Back) {
      return std::nullopt;
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
  const auto last = std::find_if(profiles_.begin(), profiles_.end(),
                                 [](const Profile& profile) { return profile.last_accessed; });
  if (last != profiles_.end()) profile_focus_ = static_cast<std::size_t>(last - profiles_.begin());

  if (items.empty()) return std::nullopt;

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

void LauncherState::set_active_profile_interface_theme(std::string theme) {
  if (active_profile_index_.has_value()) {
    profiles_[*active_profile_index_].interface_theme = std::move(theme);
  }
}

void LauncherState::set_active_profile_accent_rgb(std::uint32_t accent_rgb) noexcept {
  if (active_profile_index_.has_value()) profiles_[*active_profile_index_].accent_rgb = accent_rgb;
}

void LauncherState::move_profile_focus_vertical(int row_delta) {
  const auto profile_count = profiles_.size();
  const auto columns = profile_select_column_count(profile_count);
  const auto rows = (profile_count + columns - 1) / columns;
  if (rows <= 1) {
    return;
  }

  const auto current_row = profile_focus_ / columns;
  const auto current_column = profile_focus_ % columns;
  const auto signed_rows = static_cast<long long>(rows);
  const auto target_row = static_cast<std::size_t>(
      (static_cast<long long>(current_row) + row_delta + signed_rows) %
      signed_rows);
  const auto target_row_start = target_row * columns;
  const auto target_row_size =
      std::min(columns, profile_count - target_row_start);
  profile_focus_ =
      target_row_start + std::min(current_column, target_row_size - 1);
}

}  // namespace sprout::launcher
