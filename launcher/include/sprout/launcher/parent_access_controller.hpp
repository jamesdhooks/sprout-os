#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/parent_pin_presentation.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace sprout::launcher {

struct AccessMoment {
  std::int64_t utc_seconds;
  std::string local_date;
};

enum class ParentAccessEventType {
  ExitRequested,
  ActionInvoked,
};

struct ParentAccessEvent {
  ParentAccessEventType type;
  std::string profile_id;
  std::string target;
};

class ParentAccessController {
 public:
  ParentAccessController(LauncherState& state, ParentAccessStore* access_store,
                         std::optional<std::string> credential_ref);

  [[nodiscard]] bool has_pin_prompt() const noexcept;
  [[nodiscard]] const ParentPinPresentation& pin_prompt() const;
  [[nodiscard]] bool ensure_active_profile_access(const AccessMoment& now);
  [[nodiscard]] std::optional<ParentAccessEvent> request_exit(
      const AccessMoment& now);
  void lock_and_return_to_profiles();
  [[nodiscard]] std::optional<ParentAccessEvent> handle(Action action,
                                                        const AccessMoment& now);

 private:
  enum class PinPurpose {
    UnlockParent,
    Reauthenticate,
    AuthorizeExit,
  };

  void open_pin(PinPurpose purpose);
  void close_pin() noexcept;
  [[nodiscard]] std::optional<ParentAccessEvent> handle_pin(
      Action action, const AccessMoment& now);

  LauncherState& state_;
  ParentAccessStore* access_store_;
  std::optional<std::string> credential_ref_;
  std::unique_ptr<ParentPinPresentation> pin_;
  std::optional<PinPurpose> pin_purpose_;
  std::string pending_sensitive_target_;
};

}  // namespace sprout::launcher
