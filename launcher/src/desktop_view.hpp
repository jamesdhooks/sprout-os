#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/library_presentation.hpp"
#include "sprout/launcher/profile_image_crop_presentation.hpp"
#include "sprout/launcher/profile_archive_presentation.hpp"
#include "sprout/launcher/profile_avatar_presentation.hpp"
#include "sprout/launcher/recovery_presentation.hpp"
#include "sprout/launcher/parent_pin_presentation.hpp"
#include "sprout/launcher/setup_presentation.hpp"

#include <filesystem>

struct SDL_Renderer;

namespace sprout::launcher {

bool render_startup_splash(SDL_Renderer* renderer,
                           const std::filesystem::path& image_path);
void render_launcher(SDL_Renderer* renderer, const LauncherState& state,
                     const std::filesystem::path& managed_image_root = {},
                     const std::filesystem::path& background_image = {},
                     const std::filesystem::path& accent_atlas = {},
                     const std::filesystem::path& built_in_avatar_root = {});
void render_profile_avatars(
    SDL_Renderer* renderer, const ProfileAvatarPresentation& presentation,
    const std::filesystem::path& built_in_avatar_root);
void render_setup(SDL_Renderer* renderer, const SetupPresentation& setup);
void render_recovery(SDL_Renderer* renderer,
                     const RecoveryPresentation& recovery);
void render_profile_image_crop(SDL_Renderer* renderer,
                               const ProfileImageCropPresentation& crop);
void render_parent_pin(SDL_Renderer* renderer,
                       const ParentPinPresentation& pin);
void render_library(
    SDL_Renderer* renderer, const LibraryPresentation& library,
    const Profile* active_profile = nullptr,
    const std::filesystem::path& managed_image_root = {},
    const std::filesystem::path& built_in_avatar_root = {});
void render_profile_archive(SDL_Renderer* renderer,
                            const ProfileArchivePresentation& archive);

}  // namespace sprout::launcher
