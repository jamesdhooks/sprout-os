#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/local_library.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class LibrarySection {
  Recent,
  Favorites,
  All,
};

struct LibraryEntry {
  EmulatedLibraryItem item;
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
  std::optional<EmulatedLaunchTarget> launch_target;
  std::string message;
};

[[nodiscard]] std::optional<LibrarySection> library_section_for_menu_target(
    std::string_view target);
[[nodiscard]] std::vector<LibraryEntry> make_demo_library();

class LibraryPresentation {
 public:
  LibraryPresentation(std::vector<LibraryEntry> entries,
                      LibrarySection section);

  [[nodiscard]] LibrarySection section() const noexcept;
  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view empty_message() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::span<const LibraryEntry> entries() const noexcept;
  [[nodiscard]] std::string_view notice() const noexcept;
  [[nodiscard]] std::optional<LibraryPresentationEvent> handle(Action action);

 private:
  void move_focus(int delta);

  LibrarySection section_;
  std::vector<LibraryEntry> entries_;
  std::size_t focus_index_{0};
  std::string notice_;
};

}  // namespace sprout::launcher
