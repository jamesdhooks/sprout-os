#include "sprout/launcher/parent_access_controller.hpp"

#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

bool sensitive_target(const std::string& target) {
  return target == "Family Dashboard" || target == "Onion Tools" ||
         target == "Backup & Restore";
}

}  // namespace

ParentAccessController::ParentAccessController(
    LauncherState& state, ParentAccessStore* access_store,
    std::optional<std::string> credential_ref)
    : state_(state),
      access_store_(access_store),
      credential_ref_(std::move(credential_ref)) {
  if (credential_ref_.has_value() && access_store_ == nullptr) {
    throw std::invalid_argument("Parent credential requires an access store");
  }
}

bool ParentAccessController::has_pin_prompt() const noexcept {
  return pin_ != nullptr;
}

const ParentPinPresentation& ParentAccessController::pin_prompt() const {
  if (pin_ == nullptr) {
    throw std::logic_error("Parent PIN prompt is not active");
  }
  return *pin_;
}

bool ParentAccessController::ensure_active_profile_access(
    const AccessMoment& now) {
  if (state_.screen() != Screen::ParentHome || !credential_ref_.has_value() ||
      access_store_->is_unlocked(now.utc_seconds, now.local_date)) {
    return true;
  }
  (void)state_.handle(Action::Back);
  return false;
}

std::optional<ParentAccessEvent> ParentAccessController::handle(
    Action action, const AccessMoment& now) {
  if (pin_ != nullptr) {
    return handle_pin(action, now);
  }

  if (!ensure_active_profile_access(now)) {
    return std::nullopt;
  }

  if (state_.screen() == Screen::ProfileSelect && action == Action::Confirm) {
    const auto& focused = state_.profiles()[state_.focus_index()];
    if (focused.role == ProfileRole::Parent && credential_ref_.has_value() &&
        !access_store_->is_unlocked(now.utc_seconds, now.local_date)) {
      open_pin(PinPurpose::UnlockParent);
      return std::nullopt;
    }
  }

  const auto launcher_event = state_.handle(action);
  if (!launcher_event.has_value()) {
    return std::nullopt;
  }
  if (launcher_event->type == EventType::ExitRequested) {
    return ParentAccessEvent{
        .type = ParentAccessEventType::ExitRequested,
        .profile_id = {},
        .target = {},
    };
  }
  if (launcher_event->type != EventType::MenuItemInvoked) {
    return std::nullopt;
  }
  if (launcher_event->target == "Lock Parent Access") {
    if (access_store_ != nullptr) {
      access_store_->lock();
    }
    (void)state_.handle(Action::Back);
    return std::nullopt;
  }
  if (credential_ref_.has_value() && sensitive_target(launcher_event->target)) {
    pending_sensitive_target_ = launcher_event->target;
    open_pin(PinPurpose::Reauthenticate);
    return std::nullopt;
  }
  return ParentAccessEvent{
      .type = ParentAccessEventType::ActionInvoked,
      .profile_id = launcher_event->profile_id,
      .target = launcher_event->target,
  };
}

void ParentAccessController::open_pin(PinPurpose purpose) {
  pin_ = std::make_unique<ParentPinPresentation>(ParentPinMode::Authenticate);
  pin_purpose_ = purpose;
}

void ParentAccessController::close_pin() noexcept {
  pin_.reset();
  pin_purpose_.reset();
  pending_sensitive_target_.clear();
}

std::optional<ParentAccessEvent> ParentAccessController::handle_pin(
    Action action, const AccessMoment& now) {
  const auto event = pin_->handle(action);
  if (event == ParentPinEvent::Cancelled) {
    close_pin();
    return std::nullopt;
  }
  if (event != ParentPinEvent::Submitted) {
    return std::nullopt;
  }

  std::string pin = pin_->take_pin();
  if (*pin_purpose_ == PinPurpose::UnlockParent) {
    try {
      access_store_->grant_until_end_of_day(*credential_ref_, std::move(pin),
                                             now.utc_seconds, now.local_date);
    } catch (const std::invalid_argument&) {
      pin_->authentication_failed();
      return std::nullopt;
    }
    close_pin();
    (void)state_.handle(Action::Confirm);
    return std::nullopt;
  }

  if (!access_store_->is_unlocked(now.utc_seconds, now.local_date)) {
    close_pin();
    (void)state_.handle(Action::Back);
    return std::nullopt;
  }
  if (!access_store_->verify_pin(*credential_ref_, std::move(pin))) {
    pin_->authentication_failed();
    return std::nullopt;
  }
  ParentAccessEvent result{
      .type = ParentAccessEventType::ActionInvoked,
      .profile_id = state_.active_profile()->id,
      .target = pending_sensitive_target_,
  };
  close_pin();
  return result;
}

}  // namespace sprout::launcher
