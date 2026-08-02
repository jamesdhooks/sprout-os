#pragma once

#include "sprout/launcher/launcher_state.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

inline constexpr std::uint32_t kProfileSchemaVersion = 1;
inline constexpr std::uint32_t kProfileDatabaseSchemaVersion = 1;

enum class ProfileLifecycle {
  Active,
  Archived,
};

struct NewProfile {
  std::string id;
  std::string display_name;
  ProfileRole role;
  std::string avatar_ref;
  std::string save_namespace;
  std::optional<std::string> content_policy_ref;
  std::optional<std::string> time_policy_ref;
  std::string preferences_json{"{}"};
};

struct ProfileRecord {
  std::uint32_t schema_version;
  std::string id;
  std::string display_name;
  ProfileRole role;
  std::string avatar_ref;
  std::string save_namespace;
  std::optional<std::string> content_policy_ref;
  std::optional<std::string> time_policy_ref;
  std::string preferences_json;
  ProfileLifecycle lifecycle;
  std::uint64_t local_revision;
  std::string created_at;
  std::string updated_at;
};

class ProfileRepository {
 public:
  explicit ProfileRepository(const std::filesystem::path& database_path);
  ~ProfileRepository();

  ProfileRepository(ProfileRepository&&) noexcept;
  ProfileRepository& operator=(ProfileRepository&&) noexcept;
  ProfileRepository(const ProfileRepository&) = delete;
  ProfileRepository& operator=(const ProfileRepository&) = delete;

  [[nodiscard]] std::uint32_t database_schema_version() const;
  [[nodiscard]] std::vector<ProfileRecord> list_profiles(
      bool include_archived = true) const;
  [[nodiscard]] std::optional<ProfileRecord> find_profile(
      const std::string& id) const;

  void create_profile(const NewProfile& profile);
  void archive_profile(const std::string& id);
  void restore_profile(const std::string& id);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::launcher
