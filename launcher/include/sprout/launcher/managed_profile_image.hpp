#pragma once

#include <filesystem>
#include <string>

namespace sprout::launcher {

struct ManagedProfileImagePaths {
  std::filesystem::path directory;
  std::filesystem::path portrait;
  std::filesystem::path thumbnail;
};

[[nodiscard]] bool is_managed_profile_image_reference(
    const std::string& avatar_ref) noexcept;
[[nodiscard]] ManagedProfileImagePaths resolve_managed_profile_image(
    const std::filesystem::path& managed_image_root,
    const std::string& avatar_ref);

}  // namespace sprout::launcher
