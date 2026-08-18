#pragma once

#include "sprout/launcher/game_dashboard_model.hpp"
#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/read_only_view.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class GameDashboardStage { Dashboard, Filters, Search, Details };
enum class GameDetailPage { Overview, Review, Family };
enum class GameFilterCategory { Platform, Review, Progress, Family, Visibility };

enum class GameDashboardEventType { BackRequested, LaunchRequested, SettingsRequested };

struct GameDashboardEvent {
  GameDashboardEventType type{GameDashboardEventType::BackRequested};
  std::string item_id;
};

class GameDashboardPresentation {
 public:
  GameDashboardPresentation(GameLibraryRepository& repository,
                            std::string profile_id,
                            std::vector<Profile> child_profiles,
                            bool parent_mode = true, bool big_mode = false,
                            std::string avatar_ref = "builtin:friendly-dragon",
                            std::string background_ref = "builtin:garden-morning",
                            std::string interface_theme = "cream",
                            std::uint32_t accent_rgb = 0xE57557U,
                            bool rounded_tiles = true, bool motion_enabled = true);

  [[nodiscard]] GameDashboardStage stage() const noexcept;
  [[nodiscard]] ReadOnlyView<DashboardRow> rows() const noexcept;
  [[nodiscard]] std::size_t row_focus() const noexcept;
  [[nodiscard]] std::size_t item_focus(DashboardRowKind kind) const noexcept;
  [[nodiscard]] const GameLibraryFilter& filter() const noexcept;
  [[nodiscard]] const GameLibraryFilter& draft_filter() const noexcept;
  [[nodiscard]] GameFilterCategory filter_category() const noexcept;
  [[nodiscard]] std::size_t filter_option_focus() const noexcept;
  [[nodiscard]] ReadOnlyView<PlatformSummary> available_platforms() const noexcept;
  [[nodiscard]] ReadOnlyView<Profile> child_profiles() const noexcept;
  [[nodiscard]] GameDetailPage detail_page() const noexcept;
  [[nodiscard]] std::size_t detail_focus() const noexcept;
  [[nodiscard]] std::size_t detail_artwork_focus() const noexcept;
  [[nodiscard]] const GameLibraryRecord* selected_game() const noexcept;
  [[nodiscard]] const GameLibraryRecord* find_game(
      std::string_view item_id) const noexcept;
  [[nodiscard]] std::string_view notice() const noexcept;
  [[nodiscard]] std::string_view search_query() const noexcept;
  [[nodiscard]] ReadOnlyView<GameLibraryRecord> search_results() const noexcept;
  [[nodiscard]] std::size_t search_keyboard_focus() const noexcept;
  [[nodiscard]] std::size_t search_result_focus() const noexcept;
  [[nodiscard]] bool search_results_focused() const noexcept;
  [[nodiscard]] bool big_mode() const noexcept;
  [[nodiscard]] std::string_view profile_avatar_ref() const noexcept;
  [[nodiscard]] std::string_view profile_background_ref() const noexcept;
  [[nodiscard]] std::string_view interface_theme() const noexcept;
  [[nodiscard]] std::uint32_t accent_rgb() const noexcept;
  [[nodiscard]] bool rounded_tiles() const noexcept;
  [[nodiscard]] bool motion_enabled() const noexcept;
  void set_big_mode(bool enabled) noexcept;
  void set_profile_avatar_ref(std::string reference);
  void set_profile_background_ref(std::string reference);
  void set_interface_theme(std::string theme);
  void set_accent_rgb(std::uint32_t accent_rgb) noexcept;
  void set_rounded_tiles(bool rounded) noexcept;
  void set_motion_enabled(bool enabled) noexcept;

  [[nodiscard]] std::optional<GameDashboardEvent> handle(
      Action action, std::int64_t now);
  void report_launch_result(std::string notice);
  void refresh();

 private:
  static std::size_t row_offset(DashboardRowKind kind) noexcept;
  [[nodiscard]] std::size_t active_item_count() const noexcept;
  [[nodiscard]] std::optional<std::string> focused_game_id() const;
  void move_row(int delta);
  void move_item(int delta);
  void open_filters();
  void open_search();
  void handle_search_action(Action action, std::int64_t now);
  void rebuild_search();
  void handle_filter_action(Action action);
  void handle_detail_action(Action action, std::int64_t now);
  void toggle_review(GameReviewVerdict verdict, std::int64_t now);
  void rebuild();

  GameLibraryRepository& repository_;
  std::string profile_id_;
  std::vector<Profile> child_profiles_;
  bool parent_mode_{true};
  bool big_mode_{false};
  std::string profile_avatar_ref_;
  std::string profile_background_ref_;
  std::string interface_theme_;
  std::uint32_t accent_rgb_{0xE57557U};
  bool rounded_tiles_{true};
  bool motion_enabled_{true};
  std::vector<GameLibraryRecord> records_;
  GameDashboardModel model_;
  std::vector<DashboardRow> rows_;
  std::vector<PlatformSummary> available_platforms_;
  std::array<std::size_t, 11> item_focus_{};
  std::size_t row_focus_{0};
  GameDashboardStage stage_{GameDashboardStage::Dashboard};
  GameLibraryFilter draft_filter_;
  GameFilterCategory filter_category_{GameFilterCategory::Platform};
  std::size_t filter_option_focus_{0};
  std::optional<std::string> detail_item_id_;
  GameDetailPage detail_page_{GameDetailPage::Overview};
  std::size_t detail_focus_{0};
  std::size_t detail_artwork_focus_{0};
  std::string notice_;
  std::string search_query_;
  std::vector<GameLibraryRecord> search_results_;
  std::size_t search_keyboard_focus_{0};
  std::size_t search_result_focus_{0};
  bool search_results_focused_{false};
};

}  // namespace sprout::launcher
