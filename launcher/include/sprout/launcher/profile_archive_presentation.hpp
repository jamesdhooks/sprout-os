#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_archive.hpp"
#include "sprout/launcher/profile_repository.hpp"
#include "sprout/launcher/read_only_view.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class ProfileArchivePresentationEvent {
  BackRequested,
  ProfilesChanged,
};

class ProfileArchivePresentation {
 public:
  ProfileArchivePresentation(ProfileRepository& profiles,
                             ProfileArchiveService& archives,
                             std::filesystem::path export_directory,
                             std::filesystem::path import_directory);

  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view description() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] ReadOnlyView<std::string> choices() const noexcept;
  [[nodiscard]] std::string_view notice() const noexcept;
  [[nodiscard]] bool notice_is_error() const noexcept;
  [[nodiscard]] std::optional<ProfileArchivePresentationEvent> handle(
      Action action);

 private:
  enum class View {
    Home,
    ExportProfiles,
    ConfirmPersonalImage,
    RestoreArchives,
  };

  struct RestoreCandidate {
    std::filesystem::path path;
    ProfileArchiveSummary summary;
  };

  void show_home();
  void show_export_profiles();
  void show_restore_archives();
  void move_focus(int delta);
  void export_selected(bool include_managed_avatar);

  ProfileRepository& profiles_;
  ProfileArchiveService& archives_;
  std::filesystem::path export_directory_;
  std::filesystem::path import_directory_;
  View view_{View::Home};
  std::vector<ProfileRecord> export_profiles_;
  std::vector<RestoreCandidate> restore_candidates_;
  std::vector<std::string> choices_;
  std::size_t focus_index_{0};
  std::optional<std::size_t> pending_export_index_;
  std::string notice_;
  bool notice_is_error_{false};
};

}  // namespace sprout::launcher
