#include "sprout/launcher/parent_pin_presentation.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::ParentPinEvent;
using sprout::launcher::ParentPinMode;
using sprout::launcher::ParentPinPresentation;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void move_to(ParentPinPresentation& pin, std::size_t target) {
  while (pin.focus_index() / 3 != target / 3) {
    (void)pin.handle(Action::Down);
  }
  while (pin.focus_index() % 3 != target % 3) {
    (void)pin.handle(Action::Right);
  }
}

std::size_t digit_index(char digit) {
  return digit == '0' ? 10U : static_cast<std::size_t>(digit - '1');
}

std::optional<ParentPinEvent> enter(ParentPinPresentation& pin,
                                    std::string_view digits) {
  for (const char digit : digits) {
    move_to(pin, digit_index(digit));
    (void)pin.handle(Action::Confirm);
  }
  move_to(pin, 11);
  return pin.handle(Action::Confirm);
}

void authentication_collects_a_bounded_pin() {
  ParentPinPresentation pin(ParentPinMode::Authenticate);
  expect(pin.title() == "PARENT PIN" && pin.choices().size() == 12,
         "authentication should expose the controller keypad");
  expect(!enter(pin, "123").has_value() && !pin.error_message().empty(),
         "fewer than four digits should stay on the keypad");
  move_to(pin, 9);
  (void)pin.handle(Action::Confirm);
  expect(pin.entered_digits() == 0, "clear should remove entered digits");
  expect(enter(pin, "2468") == ParentPinEvent::Submitted,
         "valid PIN should produce an explicit submission");
  expect(pin.take_pin() == "2468", "submitted digits should be consumed once");
  pin.authentication_failed();
  expect(!pin.error_message().empty() && pin.entered_digits() == 0,
         "failed verification should reset the keypad safely");
}

void creation_requires_matching_confirmation() {
  ParentPinPresentation pin(ParentPinMode::Create);
  expect(!enter(pin, "2468").has_value() &&
             pin.title() == "CONFIRM PARENT PIN",
         "first entry should move to confirmation without submission");
  expect(!enter(pin, "1357").has_value() && !pin.error_message().empty() &&
             pin.title() == "CREATE PARENT PIN",
         "mismatch should clear both entries and restart creation");
  expect(!enter(pin, "8642").has_value(),
         "replacement first entry should request confirmation");
  expect(enter(pin, "8642") == ParentPinEvent::Submitted &&
             pin.take_pin() == "8642",
         "matching confirmation should submit the new PIN");
}

void back_cancels_without_submitting() {
  ParentPinPresentation pin(ParentPinMode::Authenticate);
  move_to(pin, 0);
  (void)pin.handle(Action::Confirm);
  expect(pin.handle(Action::Back) == ParentPinEvent::Cancelled &&
             pin.entered_digits() == 0,
         "back should cancel and clear partial input");
}

}  // namespace

int main() {
  try {
    authentication_collects_a_bounded_pin();
    creation_requires_matching_confirmation();
    back_cancels_without_submitting();
  } catch (const std::exception& error) {
    std::cerr << "parent PIN presentation test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "parent PIN presentation tests passed\n";
  return EXIT_SUCCESS;
}
