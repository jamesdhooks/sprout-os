#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace sprout::launcher {

inline constexpr std::uint32_t kLocalConfigurationSchemaVersion = 1;

enum class SetupStep {
  Welcome,
  Locale,
  Network,
  Parent,
  ParentPin,
  Child,
  Avatars,
  Library,
  ChildDefaults,
  Connectors,
  Review,
  Complete,
};

struct LocaleOverrides {
  std::optional<std::string> language;
  std::optional<std::string> region;
  std::optional<std::string> time_zone;
};

struct ResolvedLocale {
  std::string language;
  std::string region;
  std::string time_zone;
};

struct LocalConfiguration {
  std::uint32_t schema_version{kLocalConfigurationSchemaVersion};
  std::uint64_t revision{0};
  SetupStep next_setup_step{SetupStep::Welcome};
  std::string household_id{"local-household"};
  std::string device_id{"local-device"};
  bool offline_setup{true};
  LocaleOverrides household_locale;
  LocaleOverrides device_locale;
  std::map<std::string, LocaleOverrides> profile_locales;
  std::optional<std::string> parent_credential_ref;
};

[[nodiscard]] ResolvedLocale resolve_locale(
    const ResolvedLocale& platform_defaults,
    const LocalConfiguration& configuration,
    const std::optional<std::string>& profile_id = std::nullopt);

void validate_configuration(const LocalConfiguration& configuration);

class ConfigurationStore {
 public:
  explicit ConfigurationStore(std::filesystem::path configuration_directory);

  [[nodiscard]] bool has_active() const;
  [[nodiscard]] bool has_last_known_good() const;
  [[nodiscard]] LocalConfiguration load_active() const;
  [[nodiscard]] LocalConfiguration save(LocalConfiguration configuration);
  [[nodiscard]] LocalConfiguration restore_last_known_good();

  [[nodiscard]] const std::filesystem::path& active_path() const noexcept;
  [[nodiscard]] const std::filesystem::path& last_known_good_path() const noexcept;

 private:
  std::filesystem::path directory_;
  std::filesystem::path active_path_;
  std::filesystem::path last_known_good_path_;
};

}  // namespace sprout::launcher
