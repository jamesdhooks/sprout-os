#include "sprout/launcher/profile_archive_presentation.hpp"

#include "sprout/launcher/string_compat.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <system_error>
#include <utility>

namespace sprout::launcher {
namespace {

std::string hex_id(std::string_view value) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() * 2);
  for (const unsigned char character : value) {
    result.push_back(kHex[character >> 4U]);
    result.push_back(kHex[character & 0x0FU]);
  }
  return result;
}

bool archive_extension(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extension == ".sprout-profile";
}

}  // namespace

ProfileArchivePresentation::ProfileArchivePresentation(
    ProfileRepository& profiles, ProfileArchiveService& archives,
    std::filesystem::path export_directory,
    std::filesystem::path import_directory)
    : profiles_(profiles),
      archives_(archives),
      export_directory_(std::move(export_directory)),
      import_directory_(std::move(import_directory)) {
  show_home();
}

std::string_view ProfileArchivePresentation::title() const noexcept {
  switch (view_) {
    case View::Home:
      return "BACKUP AND RESTORE";
    case View::ExportProfiles:
      return "EXPORT ONE PROFILE";
    case View::ConfirmPersonalImage:
      return "INCLUDE PORTRAIT?";
    case View::RestoreArchives:
      return "RESTORE ONE PROFILE";
  }
  return "BACKUP AND RESTORE";
}

std::string_view ProfileArchivePresentation::description() const noexcept {
  switch (view_) {
    case View::Home:
      return "PORTABLE PROFILE FILES - NO SAVES OR SECRETS";
    case View::ExportProfiles:
      return "CHOOSE AN ACTIVE PROFILE TO EXPORT";
    case View::ConfirmPersonalImage:
      return "THE PORTRAIT WILL BE STORED UNENCRYPTED";
    case View::RestoreArchives:
      return "CHOOSE A VALID FILE FROM THE IMPORTS FOLDER";
  }
  return {};
}

std::size_t ProfileArchivePresentation::focus_index() const noexcept {
  return focus_index_;
}

ReadOnlyView<std::string> ProfileArchivePresentation::choices() const noexcept {
  return choices_;
}

std::string_view ProfileArchivePresentation::notice() const noexcept {
  return notice_;
}

bool ProfileArchivePresentation::notice_is_error() const noexcept {
  return notice_is_error_;
}

std::optional<ProfileArchivePresentationEvent>
ProfileArchivePresentation::handle(Action action) {
  if (action == Action::Up || action == Action::Left) {
    move_focus(-1);
    return std::nullopt;
  }
  if (action == Action::Down || action == Action::Right) {
    move_focus(1);
    return std::nullopt;
  }
  if (action == Action::Back) {
    if (view_ == View::Home) {
      return ProfileArchivePresentationEvent::BackRequested;
    }
    show_home();
    return std::nullopt;
  }
  if (action != Action::Confirm || choices_.empty()) {
    return std::nullopt;
  }

  if (view_ == View::Home) {
    if (focus_index_ == 0) {
      show_export_profiles();
    } else {
      show_restore_archives();
    }
    return std::nullopt;
  }

  if (view_ == View::ExportProfiles) {
    if (export_profiles_.empty()) {
      show_home();
      return std::nullopt;
    }
    pending_export_index_ = focus_index_;
    if (starts_with(export_profiles_[focus_index_].avatar_ref, "local:")) {
      view_ = View::ConfirmPersonalImage;
      choices_ = {"INCLUDE PORTRAIT", "CANCEL"};
      focus_index_ = 1;
      notice_.clear();
      notice_is_error_ = false;
      return std::nullopt;
    }
    export_selected(false);
    return std::nullopt;
  }

  if (view_ == View::ConfirmPersonalImage) {
    if (focus_index_ == 1) {
      show_export_profiles();
      return std::nullopt;
    }
    export_selected(true);
    return std::nullopt;
  }

  if (restore_candidates_.empty()) {
    show_home();
    return std::nullopt;
  }
  try {
    const auto restored = archives_.restore_profile(
        restore_candidates_[focus_index_].path);
    notice_ = "RESTORED " + restored.display_name;
    notice_is_error_ = false;
    return ProfileArchivePresentationEvent::ProfilesChanged;
  } catch (const std::exception& error) {
    notice_ = error.what();
    notice_is_error_ = true;
    return std::nullopt;
  }
}

void ProfileArchivePresentation::show_home() {
  view_ = View::Home;
  choices_ = {"EXPORT A PROFILE", "RESTORE AN ARCHIVE"};
  export_profiles_.clear();
  restore_candidates_.clear();
  pending_export_index_.reset();
  focus_index_ = 0;
}

void ProfileArchivePresentation::show_export_profiles() {
  view_ = View::ExportProfiles;
  export_profiles_ = profiles_.list_profiles(false);
  choices_.clear();
  for (const auto& profile : export_profiles_) {
    choices_.push_back(profile.display_name);
  }
  if (choices_.empty()) {
    choices_.push_back("NO ACTIVE PROFILES - BACK");
  }
  pending_export_index_.reset();
  focus_index_ = 0;
  notice_.clear();
  notice_is_error_ = false;
}

void ProfileArchivePresentation::show_restore_archives() {
  view_ = View::RestoreArchives;
  restore_candidates_.clear();
  choices_.clear();
  notice_.clear();
  notice_is_error_ = false;

  std::error_code error;
  if (std::filesystem::is_directory(import_directory_, error)) {
    std::filesystem::directory_iterator iterator(import_directory_, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end) {
      const auto entry = *iterator;
      std::error_code entry_error;
      const bool regular = entry.is_regular_file(entry_error);
      if (!entry_error && regular && archive_extension(entry.path())) {
        try {
          restore_candidates_.push_back(RestoreCandidate{
              .path = entry.path(),
              .summary = archives_.inspect_archive(entry.path()),
          });
        } catch (const std::exception&) {
        }
      }
      iterator.increment(error);
    }
    if (error) {
      notice_ = "IMPORTS FOLDER COULD NOT BE READ";
      notice_is_error_ = true;
    }
  }
  std::sort(restore_candidates_.begin(), restore_candidates_.end(),
            [](const RestoreCandidate& left, const RestoreCandidate& right) {
              return left.path.filename().string() < right.path.filename().string();
            });
  for (const auto& candidate : restore_candidates_) {
    choices_.push_back(candidate.summary.display_name);
  }
  if (choices_.empty()) {
    choices_.push_back("NO VALID ARCHIVES - BACK");
  }
  focus_index_ = 0;
}

void ProfileArchivePresentation::move_focus(int delta) {
  if (choices_.empty()) {
    return;
  }
  const auto count = static_cast<long long>(choices_.size());
  const auto focus = static_cast<long long>(focus_index_);
  focus_index_ = static_cast<std::size_t>((focus + delta + count) % count);
}

void ProfileArchivePresentation::export_selected(bool include_managed_avatar) {
  if (!pending_export_index_.has_value() ||
      *pending_export_index_ >= export_profiles_.size()) {
    show_home();
    return;
  }
  const auto profile = export_profiles_[*pending_export_index_];
  try {
    std::filesystem::create_directories(export_directory_);
    const auto destination = export_directory_ /
        ("profile-" + hex_id(profile.id) + ".sprout-profile");
    const auto exported = archives_.export_profile(
        profile.id, destination,
        ProfileArchiveExportOptions{
            .include_managed_avatar = include_managed_avatar,
        });
    show_home();
    notice_ = "EXPORTED " + exported.display_name;
    notice_is_error_ = false;
  } catch (const std::exception& error) {
    show_home();
    notice_ = error.what();
    notice_is_error_ = true;
  }
}

}  // namespace sprout::launcher
