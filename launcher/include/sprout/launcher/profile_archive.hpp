#pragma once

#include "sprout/launcher/daily_time_policy.hpp"
#include "sprout/launcher/profile_repository.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace sprout::launcher {

inline constexpr std::uint32_t kProfileArchiveSchemaVersion = 1;

struct ProfileArchiveExportOptions {
  bool include_managed_avatar{false};
};

struct ProfileArchiveSummary {
  std::uint32_t schema_version;
  std::string profile_id;
  std::string display_name;
  ProfileRole role;
  bool contains_personal_image;
};

class ProfileArchiveService {
 public:
  ProfileArchiveService(ProfileRepository& profiles,
                        DailyTimePolicyStore& time_policy,
                        std::filesystem::path managed_image_root);

  [[nodiscard]] ProfileArchiveSummary export_profile(
      const std::string& profile_id,
      const std::filesystem::path& destination,
      ProfileArchiveExportOptions options = {});
  [[nodiscard]] ProfileArchiveSummary inspect_archive(
      const std::filesystem::path& archive_path) const;
  [[nodiscard]] ProfileArchiveSummary restore_profile(
      const std::filesystem::path& archive_path);

 private:
  ProfileRepository& profiles_;
  DailyTimePolicyStore& time_policy_;
  std::filesystem::path managed_image_root_;
};

}  // namespace sprout::launcher
