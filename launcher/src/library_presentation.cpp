#include "sprout/launcher/library_presentation.hpp"

#include <algorithm>
#include <utility>

namespace sprout::launcher {

std::optional<LibrarySection> library_section_for_menu_target(
    std::string_view target) {
  if (target == "Continue") {
    return LibrarySection::Recent;
  }
  if (target == "Favorites") {
    return LibrarySection::Favorites;
  }
  if (target == "See All" || target == "All Games") {
    return LibrarySection::All;
  }
  if (target == "Sprout Arcade") {
    return LibrarySection::Arcade;
  }
  return std::nullopt;
}

LibraryPresentation::LibraryPresentation(std::vector<LibraryEntry> entries,
                                         LibrarySection section)
    : section_(section) {
  for (auto& entry : entries) {
    const bool included =
        section == LibrarySection::All ||
        (section == LibrarySection::Arcade &&
         std::holds_alternative<NativeLaunchTarget>(entry.launch_target)) ||
        (section == LibrarySection::Favorites && entry.favorite) ||
        (section == LibrarySection::Recent && entry.recent_rank.has_value());
    if (included) {
      entries_.push_back(std::move(entry));
    }
  }
  if (section == LibrarySection::Recent) {
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const LibraryEntry& left, const LibraryEntry& right) {
                       return *left.recent_rank < *right.recent_rank;
                     });
  }
}

LibrarySection LibraryPresentation::section() const noexcept { return section_; }

std::string_view LibraryPresentation::title() const noexcept {
  switch (section_) {
    case LibrarySection::Recent:
      return "CONTINUE PLAYING";
    case LibrarySection::Favorites:
      return "FAVORITES";
    case LibrarySection::All:
      return "ALL GAMES";
    case LibrarySection::Arcade:
      return "SPROUT ARCADE";
  }
  return "LIBRARY";
}

std::string_view LibraryPresentation::source_label() const noexcept {
  return section_ == LibrarySection::Arcade ? "LOCAL NATIVE GAMES"
                                            : "LOCAL GAME LIBRARY";
}

std::string_view LibraryPresentation::empty_message() const noexcept {
  switch (section_) {
    case LibrarySection::Recent:
      return "NO RECENT GAMES YET";
    case LibrarySection::Favorites:
      return "NO FAVORITES YET";
    case LibrarySection::All:
      return "NO SUPPORTED GAMES FOUND";
    case LibrarySection::Arcade:
      return "NO ARCADE GAMES FOUND";
  }
  return "NO GAMES FOUND";
}

std::size_t LibraryPresentation::focus_index() const noexcept {
  return focus_index_;
}

ReadOnlyView<LibraryEntry> LibraryPresentation::entries() const noexcept {
  return entries_;
}

std::string_view LibraryPresentation::notice() const noexcept { return notice_; }

void LibraryPresentation::report_launch_result(std::string message) {
  notice_ = std::move(message);
}

std::optional<LibraryPresentationEvent> LibraryPresentation::handle(Action action) {
  if (action == Action::Back) {
    return LibraryPresentationEvent{
        .type = LibraryPresentationEventType::BackRequested,
        .launch_target = std::nullopt,
        .message = {},
    };
  }
  if (action == Action::Up || action == Action::Left) {
    notice_.clear();
    move_focus(-1);
    return std::nullopt;
  }
  if (action == Action::Down || action == Action::Right) {
    notice_.clear();
    move_focus(1);
    return std::nullopt;
  }
  if (action != Action::Confirm || entries_.empty()) {
    return std::nullopt;
  }

  const auto& entry = entries_[focus_index_];
  if (!entry.launch_allowed || !entry.unavailable_reason.empty()) {
    notice_ = entry.unavailable_reason.empty()
                  ? "This game is not available for the active profile"
                  : entry.unavailable_reason;
    return LibraryPresentationEvent{
        .type = LibraryPresentationEventType::Unavailable,
        .launch_target = std::nullopt,
        .message = notice_,
    };
  }
  notice_.clear();
  auto launch_target = entry.launch_target;
  std::visit(
      [&](auto& target) { target.launch_allowed = entry.launch_allowed; },
      launch_target);
  return LibraryPresentationEvent{
      .type = LibraryPresentationEventType::LaunchRequested,
      .launch_target = std::move(launch_target),
      .message = {},
  };
}

void LibraryPresentation::move_focus(int delta) {
  if (entries_.empty()) {
    return;
  }
  const auto count = static_cast<long long>(entries_.size());
  const auto focus = static_cast<long long>(focus_index_);
  focus_index_ = static_cast<std::size_t>((focus + delta + count) % count);
}

}  // namespace sprout::launcher
