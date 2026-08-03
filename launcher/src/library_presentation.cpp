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
  for (const auto& entry : entries) {
    sections_[section_offset(LibrarySection::All)].push_back(entry);
    if (std::holds_alternative<NativeLaunchTarget>(entry.launch_target)) {
      sections_[section_offset(LibrarySection::Arcade)].push_back(entry);
    }
    if (entry.favorite) {
      sections_[section_offset(LibrarySection::Favorites)].push_back(entry);
    }
    if (entry.recent_rank.has_value()) {
      sections_[section_offset(LibrarySection::Recent)].push_back(entry);
    }
  }
  auto& recent = sections_[section_offset(LibrarySection::Recent)];
  if (!recent.empty()) {
    std::stable_sort(recent.begin(), recent.end(),
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
  return focus_index(section_);
}

std::size_t LibraryPresentation::focus_index(LibrarySection section) const noexcept {
  return focus_indices_[section_offset(section)];
}

ReadOnlyView<LibraryEntry> LibraryPresentation::entries() const noexcept {
  return entries(section_);
}

ReadOnlyView<LibraryEntry> LibraryPresentation::entries(
    LibrarySection section) const noexcept {
  return sections_[section_offset(section)];
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
  if (action == Action::Up) {
    notice_.clear();
    move_section(-1);
    return std::nullopt;
  }
  if (action == Action::Down) {
    notice_.clear();
    move_section(1);
    return std::nullopt;
  }
  if (action == Action::Left) {
    notice_.clear();
    move_focus(-1);
    return std::nullopt;
  }
  if (action == Action::Right) {
    notice_.clear();
    move_focus(1);
    return std::nullopt;
  }
  const auto active_entries = entries();
  if (action != Action::Confirm || active_entries.empty()) {
    return std::nullopt;
  }

  const auto& entry = active_entries[focus_index()];
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
  const auto active_entries = entries();
  if (active_entries.empty()) {
    return;
  }
  const auto count = static_cast<long long>(active_entries.size());
  auto& focus = focus_indices_[section_offset(section_)];
  const auto current = static_cast<long long>(focus);
  focus = static_cast<std::size_t>((current + delta + count) % count);
}

void LibraryPresentation::move_section(int delta) {
  constexpr std::array<LibrarySection, 4> order{
      LibrarySection::Recent, LibrarySection::Favorites,
      LibrarySection::All, LibrarySection::Arcade};
  const auto current = static_cast<long long>(section_offset(section_));
  section_ = order[static_cast<std::size_t>((current + delta + 4) % 4)];
}

std::size_t LibraryPresentation::section_offset(LibrarySection section) noexcept {
  switch (section) {
    case LibrarySection::Recent: return 0;
    case LibrarySection::Favorites: return 1;
    case LibrarySection::All: return 2;
    case LibrarySection::Arcade: return 3;
  }
  return 0;
}

}  // namespace sprout::launcher
