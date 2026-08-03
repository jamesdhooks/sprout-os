#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/read_only_view.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class RecoveryPresentationEvent {
  ConfigurationChanged,
  ExitRequested,
};

class RecoveryPresentation {
 public:
  RecoveryPresentation(ConfigurationStore& configuration,
                       std::uint64_t startup_attempt_id);

  [[nodiscard]] std::string_view title() const noexcept;
  [[nodiscard]] std::string_view description() const noexcept;
  [[nodiscard]] ReadOnlyView<std::string> choices() const noexcept;
  [[nodiscard]] std::size_t focus_index() const noexcept;
  [[nodiscard]] std::string_view notice() const noexcept;
  [[nodiscard]] bool notice_is_error() const noexcept;
  [[nodiscard]] std::optional<RecoveryPresentationEvent> handle(Action action);

 private:
  enum class View {
    Home,
    ConfirmRestore,
    ConfirmReset,
  };

  void show_home();
  void move_focus(int delta);
  [[nodiscard]] std::optional<RecoveryPresentationEvent> apply_restore();
  [[nodiscard]] std::optional<RecoveryPresentationEvent> apply_reset();

  ConfigurationStore& configuration_;
  std::uint64_t startup_attempt_id_;
  View view_{View::Home};
  std::optional<LocalConfiguration> last_known_good_;
  std::vector<std::string> choices_;
  std::size_t focus_index_{0};
  std::string home_notice_;
  bool home_notice_is_error_{false};
  std::string notice_;
  bool notice_is_error_{false};
};

}  // namespace sprout::launcher
