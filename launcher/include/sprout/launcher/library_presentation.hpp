#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/local_library.hpp"
#include "sprout/launcher/native_launch_adapter.hpp"
#include "sprout/launcher/read_only_view.hpp"

#include <optional>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sprout::launcher {

struct HouseholdSeed;

enum class LibrarySection {
  Recent,
  Favorites,
  All,
  Arcade,
};

using LibraryLaunchTarget =
    std::variant<EmulatedLaunchTarget, NativeLaunchTarget>;

struct LibraryEntry {
  std::string id;
  std::string title;
  std::string platform_label;
  std::filesystem::path artwork_path;
  LibraryLaunchTarget launch_target;
  bool child_visible{false};
  bool favorite{false};
  std::optional<std::size_t> recent_rank;
  bool launch_allowed{true};
  std::string unavailable_reason;
};

enum class LibraryPresentationEventType {
  BackRequested,
  LaunchRequested,
  Unavailable,
};

struct LibraryPresentationEvent {
  LibraryPresentationEventType type;
  std::optional<LibraryLaunchTarget> launch_target;
  std::string message;
};

[[nodiscard]] std::optional<LibrarySection> library_section_for_menu_target(
    std::string_view target);
void apply_seeded_profile_library_overlay(
    const HouseholdSeed& seed, std::string_view profile_id,
    bool child_profile, std::vector<LibraryEntry>& entries);
[[nodiscard]] std::vector<LibraryEntry> make_demo_library();

class LibraryPresentation {
 public:
  LibraryPresentation(std::vector<LibraryEntry> entries,
                      LibrarySection section);

  [[nodiscard]] LibrarySection section() const noexcept;
  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view source_label() const noexcept;
  [[nodiscard]] std::string_view empty_message() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::size_t focus_index(LibrarySection section) const noexcept;
  [[nodiscard]] ReadOnlyView<LibraryEntry> entries() const noexcept;
  [[nodiscard]] ReadOnlyView<LibraryEntry> entries(LibrarySection section) const noexcept;
  [[nodiscard]] std::string_view notice() const noexcept;
  void report_launch_result(std::string message);
  [[nodiscard]] std::optional<LibraryPresentationEvent> handle(Action action);

 private:
  void move_focus(int delta);
  void move_section(int delta);
  static std::size_t section_offset(LibrarySection section) noexcept;

  LibrarySection section_;
  std::array<std::vector<LibraryEntry>, 4> sections_;
  std::array<std::size_t, 4> focus_indices_{};
  std::string notice_;
};

}  // namespace sprout::launcher
