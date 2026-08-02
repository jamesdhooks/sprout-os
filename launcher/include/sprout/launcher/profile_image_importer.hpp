#pragma once

#include "sprout/launcher/profile_repository.hpp"

#include <filesystem>
#include <string>

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

class ProfileImageImporter {
 public:
  ProfileImageImporter(std::filesystem::path managed_image_root,
                       ProfileRepository& profiles);

  [[nodiscard]] ManagedProfileImage import_for_profile(
      const std::string& profile_id,
      const std::filesystem::path& source_path,
      CropSelection crop = {});

  [[nodiscard]] std::filesystem::path resolve_portrait(
      const std::string& avatar_ref) const;
  [[nodiscard]] std::filesystem::path resolve_thumbnail(
      const std::string& avatar_ref) const;

 private:
  [[nodiscard]] std::filesystem::path resolve(
      const std::string& avatar_ref, const char* filename) const;

  std::filesystem::path root_;
  ProfileRepository& profiles_;
};

}  // namespace sprout::launcher
