#include "sprout/launcher/setup_wizard.hpp"

#include <stdexcept>
#include <utility>

namespace sprout::launcher {
namespace {

bool matches(const ProfileRecord& existing, const NewProfile& requested) {
  return existing.id == requested.id &&
         existing.display_name == requested.display_name &&
         existing.role == requested.role &&
         existing.avatar_ref == requested.avatar_ref &&
         existing.save_namespace == requested.save_namespace &&
         existing.content_policy_ref == requested.content_policy_ref &&
         existing.time_policy_ref == requested.time_policy_ref &&
         existing.preferences_json == requested.preferences_json &&
         existing.lifecycle == ProfileLifecycle::Active;
}

}  // namespace

SetupWizard::SetupWizard(ConfigurationStore& configuration_store,
                         ProfileRepository& profiles)
    : configuration_store_(configuration_store),
      profiles_(profiles),
      configuration_(configuration_store_.has_active()
                         ? configuration_store_.load_active()
                         : configuration_store_.save(LocalConfiguration{})) {}

SetupStep SetupWizard::current_step() const noexcept {
  return configuration_.next_setup_step;
}

const LocalConfiguration& SetupWizard::configuration() const noexcept {
  return configuration_;
}

void SetupWizard::skip_current_step() {
  switch (current_step()) {
    case SetupStep::Welcome:
      advance_to(SetupStep::Locale);
      return;
    case SetupStep::Locale:
      advance_to(SetupStep::Network);
      return;
    case SetupStep::Network:
      configuration_.offline_setup = true;
      advance_to(SetupStep::Parent);
      return;
    case SetupStep::ParentPin:
      configuration_.parent_credential_ref.reset();
      advance_to(SetupStep::Child);
      return;
    case SetupStep::Child:
      advance_to(SetupStep::Avatars);
      return;
    case SetupStep::Avatars:
      advance_to(SetupStep::Library);
      return;
    case SetupStep::Library:
      advance_to(SetupStep::ChildDefaults);
      return;
    case SetupStep::ChildDefaults:
      advance_to(SetupStep::Connectors);
      return;
    case SetupStep::Connectors:
      advance_to(SetupStep::Review);
      return;
    case SetupStep::Parent:
      throw std::runtime_error("First parent creation cannot be skipped");
    case SetupStep::Review:
      throw std::runtime_error("Review must be finished explicitly");
    case SetupStep::Complete:
      throw std::runtime_error("Setup is already complete");
  }
}

void SetupWizard::configure_locale(LocaleOverrides household_locale) {
  require_step(SetupStep::Locale);
  configuration_.household_locale = std::move(household_locale);
  advance_to(SetupStep::Network);
}

void SetupWizard::continue_offline() {
  require_step(SetupStep::Network);
  configuration_.offline_setup = true;
  advance_to(SetupStep::Parent);
}

void SetupWizard::create_parent(const NewProfile& parent) {
  require_step(SetupStep::Parent);
  if (parent.role != ProfileRole::Parent) {
    throw std::invalid_argument("First setup profile must be a parent");
  }
  ensure_profile(parent);
  advance_to(SetupStep::ParentPin);
}

void SetupWizard::set_parent_credential_ref(
    std::optional<std::string> credential_ref) {
  require_step(SetupStep::ParentPin);
  configuration_.parent_credential_ref = std::move(credential_ref);
  advance_to(SetupStep::Child);
}

void SetupWizard::create_child(std::optional<NewProfile> child) {
  require_step(SetupStep::Child);
  if (child.has_value()) {
    if (child->role != ProfileRole::Child) {
      throw std::invalid_argument("Child setup step accepts only a child profile");
    }
    ensure_profile(*child);
  }
  advance_to(SetupStep::Avatars);
}

void SetupWizard::finish() {
  require_step(SetupStep::Review);
  bool has_active_parent = false;
  for (const auto& profile : profiles_.list_profiles(false)) {
    has_active_parent = has_active_parent || profile.role == ProfileRole::Parent;
  }
  if (!has_active_parent) {
    throw std::runtime_error("Setup cannot finish without an active parent");
  }
  advance_to(SetupStep::Complete);
}

void SetupWizard::require_step(SetupStep expected) const {
  if (current_step() != expected) {
    throw std::runtime_error("Setup operation does not match the persisted step");
  }
}

void SetupWizard::advance_to(SetupStep next) {
  configuration_.next_setup_step = next;
  configuration_ = configuration_store_.save(configuration_);
}

void SetupWizard::ensure_profile(const NewProfile& profile) {
  const auto existing = profiles_.find_profile(profile.id);
  if (!existing.has_value()) {
    profiles_.create_profile(profile);
    return;
  }
  if (!matches(*existing, profile)) {
    throw std::runtime_error(
        "Setup profile ID already exists with different profile data");
  }
}

}  // namespace sprout::launcher
