#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/setup_wizard.hpp"

#include <optional>
#include "sprout/launcher/read_only_view.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class SetupPresentationEvent {
  Completed,
  ExitRequested,
  ImportParentImageRequested,
  ConfigureParentPinRequested,
};

class SetupPresentation {
 public:
  explicit SetupPresentation(SetupWizard& wizard,
                             bool custom_image_available = false);

  [[nodiscard]] SetupStep step() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view description() const noexcept;
  [[nodiscard]] ReadOnlyView<std::string_view> choices() const noexcept;
  [[nodiscard]] const std::string& error_message() const noexcept;

  [[nodiscard]] std::optional<SetupPresentationEvent> handle(Action action);
  void complete_avatar_step();
  void report_avatar_error(std::string message);
  void complete_parent_pin_step(std::string credential_ref);

 private:
  void refresh_content();
  void move_focus(int delta);
  void confirm();

  SetupWizard& wizard_;
  bool custom_image_available_{false};
  std::size_t focus_index_{0};
  std::string_view title_;
  std::string_view description_;
  std::vector<std::string_view> choices_;
  std::string error_message_;
};

}  // namespace sprout::launcher
