#pragma once

#include "sprout/launcher/launcher_state.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sprout::launcher {

enum class ParentPinMode {
  Create,
  Authenticate,
};

enum class ParentPinEvent {
  Submitted,
  Cancelled,
};

class ParentPinPresentation {
 public:
  explicit ParentPinPresentation(ParentPinMode mode);
  ~ParentPinPresentation();

  ParentPinPresentation(const ParentPinPresentation&) = delete;
  ParentPinPresentation& operator=(const ParentPinPresentation&) = delete;

  [[nodiscard]] ParentPinMode mode() const noexcept;
  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view description() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::size_t entered_digits() const noexcept;
  [[nodiscard]] std::span<const std::string_view> choices() const noexcept;
  [[nodiscard]] const std::string& error_message() const noexcept;
  [[nodiscard]] std::optional<ParentPinEvent> handle(Action action);
  [[nodiscard]] std::string take_pin();
  void authentication_failed();

 private:
  void move_focus(int horizontal, int vertical) noexcept;
  [[nodiscard]] std::optional<ParentPinEvent> confirm();
  void clear_pin() noexcept;

  ParentPinMode mode_;
  bool confirming_{false};
  std::size_t focus_index_{0};
  std::string pin_;
  std::string first_pin_;
  std::string completed_pin_;
  std::string error_message_;
};

}  // namespace sprout::launcher
