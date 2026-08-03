#pragma once

#include "sprout/launcher/daily_time_policy.hpp"
#include "sprout/launcher/local_configuration.hpp"
#include "sprout/launcher/profile_repository.hpp"

#include <optional>
#include <string>

namespace sprout::launcher {

class SetupWizard {
 public:
  SetupWizard(ConfigurationStore& configuration_store,
              ProfileRepository& profiles,
              DailyTimePolicyStore* time_policy = nullptr);

  [[nodiscard]] SetupStep current_step() const noexcept;
  [[nodiscard]] const LocalConfiguration& configuration() const noexcept;

  void skip_current_step();
  void configure_locale(LocaleOverrides household_locale);
  void continue_offline();
  void create_parent(const NewProfile& parent);
  void set_parent_credential_ref(std::optional<std::string> credential_ref);
  void create_child(std::optional<NewProfile> child);
  void finish();

 private:
  void require_step(SetupStep expected) const;
  void advance_to(SetupStep next);
  void ensure_profile(const NewProfile& profile);

  ConfigurationStore& configuration_store_;
  ProfileRepository& profiles_;
  DailyTimePolicyStore* time_policy_;
  LocalConfiguration configuration_;
};

}  // namespace sprout::launcher
