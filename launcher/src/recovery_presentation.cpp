#include "sprout/launcher/recovery_presentation.hpp"

#include <exception>
#include <stdexcept>

namespace sprout::launcher {

RecoveryPresentation::RecoveryPresentation(ConfigurationStore& configuration,
                                             std::uint64_t startup_attempt_id)
    : configuration_(configuration),
      startup_attempt_id_(startup_attempt_id) {
  if (startup_attempt_id_ == 0) {
    throw std::invalid_argument("Recovery requires a startup attempt identity");
  }
  if (configuration_.has_last_known_good()) {
    try {
      last_known_good_ = configuration_.load_last_known_good();
    } catch (const std::exception&) {
      home_notice_ = "LAST-KNOWN-GOOD CONFIGURATION IS INVALID";
      home_notice_is_error_ = true;
    }
  } else {
    home_notice_ = "NO LAST-KNOWN-GOOD CONFIGURATION IS AVAILABLE";
  }
  show_home();
}

std::string_view RecoveryPresentation::title() const noexcept {
  switch (view_) {
    case View::Home:
      return "SPROUT RECOVERY";
    case View::ConfirmRestore:
      return "RESTORE CONFIGURATION?";
    case View::ConfirmReset:
      return "RESET LAUNCHER SETUP?";
  }
  return "SPROUT RECOVERY";
}

std::string_view RecoveryPresentation::description() const noexcept {
  if (view_ == View::ConfirmRestore && last_known_good_.has_value()) {
    return "VALIDATED SNAPSHOT WILL REPLACE ACTIVE CONFIG";
  }
  if (view_ == View::ConfirmReset) {
    return "ACTIVE CONFIG IS KEPT IN THE RECOVERY FOLDER";
  }
  return "REPEATED STARTUP FAILURES NEED A SAFE CHOICE";
}

ReadOnlyView<std::string> RecoveryPresentation::choices() const noexcept {
  return choices_;
}

std::size_t RecoveryPresentation::focus_index() const noexcept {
  return focus_index_;
}

std::string_view RecoveryPresentation::notice() const noexcept {
  return notice_;
}

bool RecoveryPresentation::notice_is_error() const noexcept {
  return notice_is_error_;
}

std::optional<RecoveryPresentationEvent> RecoveryPresentation::handle(
    Action action) {
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
      return RecoveryPresentationEvent::ExitRequested;
    }
    show_home();
    return std::nullopt;
  }
  if (action != Action::Confirm || choices_.empty()) {
    return std::nullopt;
  }

  if (view_ == View::Home) {
    const std::string selected = choices_[focus_index_];
    if (selected == "RESTORE LAST-KNOWN-GOOD") {
      view_ = View::ConfirmRestore;
      choices_ = {"RESTORE", "CANCEL"};
      focus_index_ = 1;
      notice_ = "REVISION " + std::to_string(last_known_good_->revision) +
                " - NEXT " +
                std::string(setup_step_name(last_known_good_->next_setup_step));
      notice_is_error_ = false;
      return std::nullopt;
    }
    if (selected == "RESET LAUNCHER SETUP") {
      view_ = View::ConfirmReset;
      choices_ = {"RESET", "CANCEL"};
      focus_index_ = 1;
      notice_ = "PROFILES, SAVES, GAMES, AND BACKUPS ARE NOT CHANGED";
      notice_is_error_ = false;
      return std::nullopt;
    }
    return RecoveryPresentationEvent::ExitRequested;
  }

  if (focus_index_ == 1) {
    show_home();
    return std::nullopt;
  }
  return view_ == View::ConfirmRestore ? apply_restore() : apply_reset();
}

void RecoveryPresentation::show_home() {
  view_ = View::Home;
  choices_.clear();
  if (last_known_good_.has_value()) {
    choices_.push_back("RESTORE LAST-KNOWN-GOOD");
  }
  choices_.push_back("RESET LAUNCHER SETUP");
  choices_.push_back("EXIT TO POWER OFF");
  focus_index_ = choices_.size() - 1;
  notice_ = home_notice_;
  notice_is_error_ = home_notice_is_error_;
}

void RecoveryPresentation::move_focus(int delta) {
  if (choices_.empty()) {
    return;
  }
  const auto count = static_cast<long long>(choices_.size());
  const auto focus = static_cast<long long>(focus_index_);
  focus_index_ = static_cast<std::size_t>((focus + delta + count) % count);
}

std::optional<RecoveryPresentationEvent>
RecoveryPresentation::apply_restore() {
  try {
    (void)configuration_.restore_last_known_good();
    return RecoveryPresentationEvent::ConfigurationChanged;
  } catch (const std::exception& error) {
    notice_ = error.what();
    notice_is_error_ = true;
    return std::nullopt;
  }
}

std::optional<RecoveryPresentationEvent> RecoveryPresentation::apply_reset() {
  try {
    (void)configuration_.quarantine_active(startup_attempt_id_);
    (void)configuration_.save(LocalConfiguration{});
    return RecoveryPresentationEvent::ConfigurationChanged;
  } catch (const std::exception& error) {
    notice_ = error.what();
    notice_is_error_ = true;
    return std::nullopt;
  }
}

}  // namespace sprout::launcher
