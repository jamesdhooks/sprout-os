#include "sprout/launcher/parent_pin_presentation.hpp"

#include <array>
#include <algorithm>
#include <stdexcept>

namespace sprout::launcher {
namespace {

constexpr std::array<std::string_view, 12> kChoices{
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "CLEAR", "0", "DONE",
};

void secure_clear(std::string& value) noexcept {
  volatile char* bytes = value.empty() ? nullptr : value.data();
  for (std::size_t index = 0; index < value.size(); ++index) {
    bytes[index] = 0;
  }
  value.clear();
}

}  // namespace

ParentPinPresentation::ParentPinPresentation(ParentPinMode mode) : mode_(mode) {}

ParentPinPresentation::~ParentPinPresentation() {
  secure_clear(pin_);
  secure_clear(first_pin_);
  secure_clear(completed_pin_);
}

ParentPinMode ParentPinPresentation::mode() const noexcept { return mode_; }

std::string_view ParentPinPresentation::title() const noexcept {
  if (mode_ == ParentPinMode::Authenticate) {
    return "PARENT PIN";
  }
  if (mode_ == ParentPinMode::Updated) return "PIN UPDATED";
  if (mode_ == ParentPinMode::ComboAuthenticate) return "PARENT COMBO";
  if (mode_ == ParentPinMode::ComboCreate) return ready_to_save_ ? "SAVE BUTTON COMBO" : confirming_ ? "CONFIRM COMBO" : "SET BUTTON COMBO";
  if (mode_ == ParentPinMode::Change) {
    return confirming_ ? "CONFIRM NEW PIN" : "SET NEW PIN";
  }
  return confirming_ ? "CONFIRM PARENT PIN" : "CREATE PARENT PIN";
}

std::string_view ParentPinPresentation::description() const noexcept {
  if (mode_ == ParentPinMode::Authenticate) {
    return "UNLOCK PARENT ACCESS UNTIL END OF DAY";
  }
  if (mode_ == ParentPinMode::Updated) return "PARENT PROFILE IS PROTECTED";
  if (mode_ == ParentPinMode::ComboAuthenticate) return "ENTER YOUR BUTTON COMBINATION";
  if (mode_ == ParentPinMode::ComboCreate) return ready_to_save_ ? "COMBO CONFIRMED" : confirming_ ? "ENTER THE SAME BUTTONS AGAIN" : "PRESS 4 BUTTONS";
  if (mode_ == ParentPinMode::Change) {
    return confirming_ ? "ENTER THE NEW PIN AGAIN" : "CHOOSE 4 TO 8 DIGITS";
  }
  return confirming_ ? "ENTER THE SAME PIN AGAIN" : "CHOOSE 4 TO 8 DIGITS";
}

bool ParentPinPresentation::is_confirmation() const noexcept {
  return mode_ == ParentPinMode::Updated;
}
bool ParentPinPresentation::uses_button_combo() const noexcept { return mode_ == ParentPinMode::ComboAuthenticate || mode_ == ParentPinMode::ComboCreate; }
bool ParentPinPresentation::ready_to_save() const noexcept { return ready_to_save_; }
bool ParentPinPresentation::completion_pending() const noexcept { return completion_pending_; }
float ParentPinPresentation::expiry_fraction() const noexcept {
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - last_input_).count();
  return std::max(0.0F, 1.0F - static_cast<float>(elapsed) / 20000.0F);
}

std::size_t ParentPinPresentation::focus_index() const noexcept {
  return focus_index_;
}

std::size_t ParentPinPresentation::entered_digits() const noexcept {
  return pin_.size();
}

ReadOnlyView<std::string_view> ParentPinPresentation::choices() const noexcept {
  return kChoices;
}

const std::string& ParentPinPresentation::error_message() const noexcept {
  return error_message_;
}

std::optional<ParentPinEvent> ParentPinPresentation::handle(Action action) {
  last_input_ = std::chrono::steady_clock::now();
  if (mode_ == ParentPinMode::Updated) {
    return (action == Action::Confirm || action == Action::Back)
               ? std::optional<ParentPinEvent>{ParentPinEvent::Cancelled}
               : std::nullopt;
  }
  if (uses_button_combo()) {
    if (completion_pending_) return std::nullopt;
    if (ready_to_save_) {
      if (action == Action::Back) return ParentPinEvent::Cancelled;
      return action == Action::Confirm
                 ? std::optional<ParentPinEvent>{ParentPinEvent::Submitted}
                 : std::nullopt;
    }
    const char button = action == Action::Up ? '1' : action == Action::Down ? '2' : action == Action::Left ? '3' : action == Action::Right ? '4' : action == Action::Confirm ? '5' : action == Action::Back ? '6' : action == Action::Filters ? '7' : action == Action::ClearFilters ? '8' : action == Action::Menu ? '9' : action == Action::ProfileSelect ? 'A' : action == Action::ZoomIn ? 'B' : action == Action::ZoomOut ? 'C' : action == Action::GameSwitcher ? 'D' : 0;
    if (button != 0) {
      pin_.push_back(button);
      if (pin_.size() == 4) {
        completion_pending_ = true;
        completion_started_ = std::chrono::steady_clock::now();
      }
    }
    return std::nullopt;
  }
  error_message_.clear();
  switch (action) {
    case Action::Left: move_focus(-1, 0); return std::nullopt;
    case Action::Right: move_focus(1, 0); return std::nullopt;
    case Action::Up: move_focus(0, -1); return std::nullopt;
    case Action::Down: move_focus(0, 1); return std::nullopt;
    case Action::Confirm: return confirm();
    case Action::Back:
      clear_pin();
      return ParentPinEvent::Cancelled;
    case Action::ZoomIn:
    case Action::ZoomOut:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<ParentPinEvent> ParentPinPresentation::advance_after_input_delay() {
  if (!completion_pending_ ||
      std::chrono::steady_clock::now() - completion_started_ < std::chrono::milliseconds(550)) {
    return std::nullopt;
  }
  completion_pending_ = false;
  if (mode_ == ParentPinMode::ComboAuthenticate) {
    completed_pin_ = pin_;
    secure_clear(pin_);
    return ParentPinEvent::Submitted;
  }
  if (!confirming_) {
    first_pin_ = pin_;
    secure_clear(pin_);
    confirming_ = true;
  } else if (pin_ != first_pin_) {
    clear_pin(); secure_clear(first_pin_); confirming_ = false;
    error_message_ = "COMBOS DID NOT MATCH - TRY AGAIN";
  } else {
    completed_pin_ = pin_;
    ready_to_save_ = true;
  }
  return std::nullopt;
}

std::string ParentPinPresentation::take_pin() {
  if (completed_pin_.empty()) {
    throw std::logic_error("No completed parent PIN is available");
  }
  std::string result = completed_pin_;
  secure_clear(completed_pin_);
  return result;
}

void ParentPinPresentation::authentication_failed() {
  clear_pin();
  error_message_ = "PIN DID NOT MATCH";
}

void ParentPinPresentation::move_focus(int horizontal, int vertical) noexcept {
  const int row = static_cast<int>(focus_index_ / 3);
  const int column = static_cast<int>(focus_index_ % 3);
  const int next_row = (row + vertical + 4) % 4;
  const int next_column = (column + horizontal + 3) % 3;
  focus_index_ = static_cast<std::size_t>(next_row * 3 + next_column);
}

std::optional<ParentPinEvent> ParentPinPresentation::confirm() {
  const auto choice = kChoices[focus_index_];
  if (choice == "CLEAR") {
    clear_pin();
    return std::nullopt;
  }
  if (choice != "DONE") {
    if (pin_.size() >= 8) {
      error_message_ = "PIN CAN USE AT MOST 8 DIGITS";
      return std::nullopt;
    }
    pin_.append(choice);
    return std::nullopt;
  }
  if (pin_.size() < 4) {
    error_message_ = "ENTER AT LEAST 4 DIGITS";
    return std::nullopt;
  }
  if ((mode_ == ParentPinMode::Create || mode_ == ParentPinMode::Change || mode_ == ParentPinMode::ComboCreate) && !confirming_) {
    first_pin_ = pin_;
    secure_clear(pin_);
    confirming_ = true;
    focus_index_ = 0;
    return std::nullopt;
  }
  if ((mode_ == ParentPinMode::Create || mode_ == ParentPinMode::Change || mode_ == ParentPinMode::ComboCreate) && pin_ != first_pin_) {
    clear_pin();
    secure_clear(first_pin_);
    confirming_ = false;
    focus_index_ = 0;
    error_message_ = "PINS DID NOT MATCH - TRY AGAIN";
    return std::nullopt;
  }
  completed_pin_ = pin_;
  secure_clear(pin_);
  secure_clear(first_pin_);
  return ParentPinEvent::Submitted;
}

void ParentPinPresentation::clear_pin() noexcept { secure_clear(pin_); }

}  // namespace sprout::launcher
