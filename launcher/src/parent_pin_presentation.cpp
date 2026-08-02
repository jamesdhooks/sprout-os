#include "sprout/launcher/parent_pin_presentation.hpp"

#include <array>
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
  return confirming_ ? "CONFIRM PARENT PIN" : "CREATE PARENT PIN";
}

std::string_view ParentPinPresentation::description() const noexcept {
  if (mode_ == ParentPinMode::Authenticate) {
    return "UNLOCK PARENT ACCESS UNTIL END OF DAY";
  }
  return confirming_ ? "ENTER THE SAME PIN AGAIN" : "CHOOSE 4 TO 8 DIGITS";
}

std::size_t ParentPinPresentation::focus_index() const noexcept {
  return focus_index_;
}

std::size_t ParentPinPresentation::entered_digits() const noexcept {
  return pin_.size();
}

std::span<const std::string_view> ParentPinPresentation::choices() const noexcept {
  return kChoices;
}

const std::string& ParentPinPresentation::error_message() const noexcept {
  return error_message_;
}

std::optional<ParentPinEvent> ParentPinPresentation::handle(Action action) {
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
  if (mode_ == ParentPinMode::Create && !confirming_) {
    first_pin_ = pin_;
    secure_clear(pin_);
    confirming_ = true;
    focus_index_ = 0;
    return std::nullopt;
  }
  if (mode_ == ParentPinMode::Create && pin_ != first_pin_) {
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
