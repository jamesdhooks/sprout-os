#include "sprout/launcher/managed_profile_image.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace sprout::launcher {
namespace {

bool safe_asset_id(std::string_view value) noexcept {
  return !value.empty() &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f') || character == '-' ||
                  character == 'r';
         });
}

}  // namespace

bool is_managed_profile_image_reference(
    const std::string& avatar_ref) noexcept {
  return starts_with(avatar_ref, "local:") &&
         safe_asset_id(std::string_view(avatar_ref).substr(6));
}

ManagedProfileImagePaths resolve_managed_profile_image(
    const std::filesystem::path& managed_image_root,
    const std::string& avatar_ref) {
  if (!is_managed_profile_image_reference(avatar_ref)) {
    throw std::invalid_argument("Avatar reference is not a managed local image");
  }
  const std::filesystem::path directory =
      managed_image_root / std::string_view(avatar_ref).substr(6);
  return ManagedProfileImagePaths{
      .directory = directory,
      .portrait = directory / "portrait.png",
      .thumbnail = directory / "thumbnail.png",
  };
}

}  // namespace sprout::launcher
