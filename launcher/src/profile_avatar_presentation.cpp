#include "sprout/launcher/profile_avatar_presentation.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::size_t kAvatarsPerPage = 8;
constexpr std::array<std::uint32_t, 30> kAccentColors{{
    0xE57557U, 0xF07E48U, 0xF0A74CU, 0xE4CC55U, 0xBFCB59U,
    0x8DBE5BU, 0x52B66CU, 0x52B6A1U, 0x4EB6BFU, 0x55A6D9U,
    0x4D82D6U, 0x6976D8U, 0x8A78D6U, 0x9B78D0U, 0xB875C4U,
    0xD274ACU, 0xD86F87U, 0xC85D5DU, 0xA84E55U, 0x814B56U,
    0xF5F1E6U, 0xDDD5C7U, 0xBFC3C8U, 0x939BA5U, 0x707985U,
    0x424850U, 0x2D3239U, 0xFFFFFFU, 0x12161CU, 0x8E6E4DU}};

std::uint32_t stored_accent(std::string_view preferences) {
  constexpr std::string_view key = "\"accent_rgb\"";
  const auto key_position = preferences.find(key);
  const auto colon = preferences.find(':', key_position);
  if (key_position == std::string_view::npos || colon == std::string_view::npos) return kAccentColors.front();
  const auto end = preferences.find_first_of(",}", colon + 1);
  try { return static_cast<std::uint32_t>(std::stoul(std::string(preferences.substr(colon + 1, end - colon - 1)))); }
  catch (...) { return kAccentColors.front(); }
}

std::string with_accent(std::string preferences, std::uint32_t accent) {
  constexpr std::string_view key = "\"accent_rgb\"";
  const auto key_position = preferences.find(key);
  if (key_position == std::string::npos) {
    const auto close = preferences.rfind('}');
    preferences.insert(close == std::string::npos ? preferences.size() : close,
                       (preferences.size() > 2 ? "," : "") +
                           std::string{"\"accent_rgb\":"} + std::to_string(accent));
    return preferences;
  }
  const auto colon = preferences.find(':', key_position);
  const auto end = preferences.find_first_of(",}", colon + 1);
  preferences.replace(colon + 1, end - colon - 1, std::to_string(accent));
  return preferences;
}

}  // namespace

ProfileAvatarPresentation::ProfileAvatarPresentation(
    ProfileRepository& profiles, bool custom_image_available,
    std::optional<std::string> initial_profile_id,
    ProfileAvatarStage initial_stage)
    : repository_(profiles),
      profiles_(profiles.list_profiles(false)),
      custom_image_available_(custom_image_available),
      profile_locked_(initial_profile_id.has_value()) {
  if (profiles_.empty()) {
    throw std::invalid_argument("Profile image settings require an active profile");
  }
  if (initial_profile_id.has_value()) {
    bool found = false;
    for (std::size_t index = 0; index < profiles_.size(); ++index) {
      if (profiles_[index].id == *initial_profile_id) {
        profile_focus_ = index;
        found = true;
        break;
      }
    }
    if (!found) {
      throw std::invalid_argument("Selected profile does not exist");
    }
    if (initial_stage != ProfileAvatarStage::Avatar &&
        initial_stage != ProfileAvatarStage::Background &&
        initial_stage != ProfileAvatarStage::Accent) {
      throw std::invalid_argument(
          "Locked profile appearance must open an avatar or background picker");
    }
    stage_ = initial_stage;
    if (stage_ == ProfileAvatarStage::Background) {
      const auto& reference = profiles_[profile_focus_].background_ref;
      constexpr std::string_view prefix = "builtin:";
      if (reference.rfind(prefix, 0) == 0) {
        const auto choices = built_in_backgrounds();
        for (std::size_t index = 0; index < choices.size(); ++index) {
          if (choices[index].id == reference.substr(prefix.size())) {
            background_focus_ = index;
            break;
          }
        }
      }
    } else if (stage_ == ProfileAvatarStage::Accent) {
      const auto accent = stored_accent(profiles_[profile_focus_].preferences_json);
      for (std::size_t index = 0; index < kAccentColors.size(); ++index) {
        if (kAccentColors[index] == accent) { accent_focus_ = index; break; }
      }
    }
  }
}

ProfileAvatarStage ProfileAvatarPresentation::stage() const noexcept {
  return stage_;
}

ReadOnlyView<ProfileRecord> ProfileAvatarPresentation::profiles() const noexcept {
  return profiles_;
}

const ProfileRecord* ProfileAvatarPresentation::selected_profile() const noexcept {
  return profiles_.empty() ? nullptr : &profiles_[profile_focus_];
}

ReadOnlyView<BuiltInAvatar> ProfileAvatarPresentation::avatars() const noexcept {
  return built_in_avatars();
}

ReadOnlyView<BuiltInBackground>
ProfileAvatarPresentation::backgrounds() const noexcept {
  return built_in_backgrounds();
}

std::uint32_t ProfileAvatarPresentation::accent_rgb() const noexcept {
  return kAccentColors[accent_focus_];
}

std::size_t ProfileAvatarPresentation::focus_index() const noexcept {
  switch (stage_) {
    case ProfileAvatarStage::Profile: return profile_focus_;
    case ProfileAvatarStage::Appearance: return appearance_focus_;
    case ProfileAvatarStage::Avatar: return avatar_focus_;
    case ProfileAvatarStage::Background: return background_focus_;
    case ProfileAvatarStage::Accent: return accent_focus_;
  }
  return 0;
}

std::size_t ProfileAvatarPresentation::page_index() const noexcept {
  return avatar_focus_ / kAvatarsPerPage;
}

std::size_t ProfileAvatarPresentation::page_count() const noexcept {
  const std::size_t count = built_in_avatars().size() +
                            (custom_image_available_ ? 1U : 0U);
  return (count + kAvatarsPerPage - 1U) / kAvatarsPerPage;
}

std::size_t ProfileAvatarPresentation::avatar_first_column() const noexcept {
  return avatar_first_column_;
}

bool ProfileAvatarPresentation::custom_image_available() const noexcept {
  return custom_image_available_;
}

bool ProfileAvatarPresentation::import_focused() const noexcept {
  return stage_ == ProfileAvatarStage::Avatar && custom_image_available_ &&
         avatar_focus_ == built_in_avatars().size();
}

const std::string& ProfileAvatarPresentation::notice() const noexcept {
  return notice_;
}

std::optional<ProfileAvatarEvent> ProfileAvatarPresentation::handle(
    Action action) {
  notice_.clear();
  if (stage_ == ProfileAvatarStage::Profile) {
    if (action == Action::Back) {
      return ProfileAvatarEvent{ProfileAvatarEventType::BackRequested, {}};
    }
    if (action == Action::Up || action == Action::Left) {
      move_profile_focus(-1);
    } else if (action == Action::Down || action == Action::Right) {
      move_profile_focus(1);
    } else if (action == Action::Confirm) {
      stage_ = ProfileAvatarStage::Appearance;
      appearance_focus_ = 0;
    }
    return std::nullopt;
  }

  if (stage_ == ProfileAvatarStage::Appearance) {
    if (action == Action::Back) {
      stage_ = ProfileAvatarStage::Profile;
    } else if (action == Action::Left || action == Action::Up) {
      appearance_focus_ = (appearance_focus_ + 1U) % 2U;
    } else if (action == Action::Right || action == Action::Down) {
      appearance_focus_ = (appearance_focus_ + 1U) % 2U;
    } else if (action == Action::Confirm) {
      stage_ = appearance_focus_ == 0 ? ProfileAvatarStage::Avatar
                                     : ProfileAvatarStage::Background;
    }
    return std::nullopt;
  }

  if (action == Action::Back) {
    if (profile_locked_) {
      return ProfileAvatarEvent{ProfileAvatarEventType::BackRequested, {}};
    }
    stage_ = ProfileAvatarStage::Appearance;
    return std::nullopt;
  }
  if (stage_ == ProfileAvatarStage::Background) {
    if (action == Action::Left) move_background_focus(-2);
    else if (action == Action::Right) move_background_focus(2);
    else if (action == Action::Up) move_background_focus(-1);
    else if (action == Action::Down) move_background_focus(1);
    else if (action == Action::Confirm) {
      const auto* profile = selected_profile();
      const auto choices = built_in_backgrounds();
      if (profile == nullptr || background_focus_ >= choices.size()) {
        throw std::logic_error("Profile background selection lost its choice");
      }
      repository_.set_background_ref(
          profile->id, built_in_background_ref(choices[background_focus_].id));
      profiles_[profile_focus_].background_ref =
          built_in_background_ref(choices[background_focus_].id);
      notice_ = "BACKGROUND UPDATED";
      return ProfileAvatarEvent{ProfileAvatarEventType::BackgroundAssigned,
                                profile->id};
    }
    return std::nullopt;
  }
  if (stage_ == ProfileAvatarStage::Accent) {
    if (action == Action::Left) move_accent_focus(-1);
    else if (action == Action::Right) move_accent_focus(1);
    else if (action == Action::Up) move_accent_focus(-10);
    else if (action == Action::Down) move_accent_focus(10);
    else if (action == Action::Confirm) {
      const auto* profile = selected_profile();
      if (profile == nullptr) throw std::logic_error("Profile accent selection lost its profile");
      auto preferences = with_accent(profile->preferences_json, accent_rgb());
      repository_.set_preferences_json(profile->id, preferences);
      profiles_[profile_focus_].preferences_json = std::move(preferences);
      return ProfileAvatarEvent{ProfileAvatarEventType::AccentAssigned, profile->id};
    }
    return std::nullopt;
  }
  if (action == Action::Left) {
    move_avatar_focus(-3);
    return std::nullopt;
  }
  if (action == Action::Right) {
    move_avatar_focus(3);
    return std::nullopt;
  }
  if (action == Action::Up) {
    move_avatar_focus(-1);
    return std::nullopt;
  }
  if (action == Action::Down) {
    move_avatar_focus(1);
    return std::nullopt;
  }
  if (action != Action::Confirm) return std::nullopt;

  const auto* profile = selected_profile();
  if (profile == nullptr) {
    throw std::logic_error("Profile avatar selection lost its profile");
  }
  if (import_focused()) {
    return ProfileAvatarEvent{ProfileAvatarEventType::ImportRequested,
                              profile->id};
  }
  const auto choices = built_in_avatars();
  if (avatar_focus_ >= choices.size()) {
    throw std::logic_error("Profile avatar focus is outside the catalogue");
  }
  repository_.set_avatar_ref(profile->id,
                             built_in_avatar_ref(choices[avatar_focus_].id));
  profiles_[profile_focus_].avatar_ref =
      built_in_avatar_ref(choices[avatar_focus_].id);
  notice_ = "PROFILE IMAGE UPDATED";
  return ProfileAvatarEvent{ProfileAvatarEventType::AvatarAssigned,
                            profile->id};
}

void ProfileAvatarPresentation::move_background_focus(int delta) {
  const auto count = static_cast<long long>(built_in_backgrounds().size());
  const auto current = static_cast<long long>(background_focus_);
  background_focus_ = static_cast<std::size_t>(
      (current + static_cast<long long>(delta) + count) % count);
}

void ProfileAvatarPresentation::move_profile_focus(int delta) {
  const auto count = static_cast<long long>(profiles_.size());
  const auto current = static_cast<long long>(profile_focus_);
  profile_focus_ = static_cast<std::size_t>((current + delta + count) % count);
}

void ProfileAvatarPresentation::move_avatar_focus(int delta) {
  const auto count = static_cast<long long>(
      built_in_avatars().size() + (custom_image_available_ ? 1U : 0U));
  const auto current = static_cast<long long>(avatar_focus_);
  avatar_focus_ = static_cast<std::size_t>(
      (current + static_cast<long long>(delta) + count) % count);
  constexpr std::size_t kRows = 3;
  constexpr std::size_t kVisibleColumns = 3;
  const auto columns = (static_cast<std::size_t>(count) + kRows - 1U) / kRows;
  const auto selected_column = avatar_focus_ / kRows;
  if (selected_column < avatar_first_column_) {
    avatar_first_column_ = selected_column;
  } else if (selected_column >= avatar_first_column_ + kVisibleColumns) {
    avatar_first_column_ = selected_column - (kVisibleColumns - 1U);
  }
  if (avatar_first_column_ >= columns) avatar_first_column_ = 0;
}

void ProfileAvatarPresentation::move_accent_focus(int delta) {
  const auto count = static_cast<long long>(kAccentColors.size());
  accent_focus_ = static_cast<std::size_t>((static_cast<long long>(accent_focus_) + delta + count) % count);
}

}  // namespace sprout::launcher
