#pragma once

#include "sprout/launcher/profile_repository.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sprout::launcher {

struct CropSelection {
  double center_x{0.5};
  double center_y{0.5};
  double zoom{1.0};
};

struct ManagedProfileImage {
  std::string avatar_ref;
  std::filesystem::path portrait_path;
  std::filesystem::path thumbnail_path;
};

class ProfileImageCropSession {
 public:
  explicit ProfileImageCropSession(const std::filesystem::path& source_path);
  ~ProfileImageCropSession();

  ProfileImageCropSession(ProfileImageCropSession&&) noexcept;
  ProfileImageCropSession& operator=(ProfileImageCropSession&&) noexcept;
  ProfileImageCropSession(const ProfileImageCropSession&) = delete;
  ProfileImageCropSession& operator=(const ProfileImageCropSession&) = delete;

  void move(double horizontal, double vertical) noexcept;
  void adjust_zoom(double delta) noexcept;
  [[nodiscard]] CropSelection selection() const noexcept;
  [[nodiscard]] std::vector<std::uint8_t> preview_rgba() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  friend class ProfileImageImporter;
};

class ProfileImageImporter {
 public:
  ProfileImageImporter(std::filesystem::path managed_image_root,
                       ProfileRepository& profiles);

  [[nodiscard]] ManagedProfileImage import_for_profile(
      const std::string& profile_id,
      const std::filesystem::path& source_path,
      CropSelection crop = {});
  [[nodiscard]] ManagedProfileImage import_for_profile(
      const std::string& profile_id, const ProfileImageCropSession& session);

  [[nodiscard]] std::filesystem::path resolve_portrait(
      const std::string& avatar_ref) const;
  [[nodiscard]] std::filesystem::path resolve_thumbnail(
      const std::string& avatar_ref) const;
  [[nodiscard]] static std::filesystem::path resolve_portrait_at(
      const std::filesystem::path& managed_image_root,
      const std::string& avatar_ref);

 private:
  [[nodiscard]] std::filesystem::path resolve(
      const std::string& avatar_ref, const char* filename) const;

  std::filesystem::path root_;
  ProfileRepository& profiles_;
};

}  // namespace sprout::launcher
