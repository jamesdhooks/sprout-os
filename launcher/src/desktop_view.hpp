#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_image_crop_presentation.hpp"
#include "sprout/launcher/parent_pin_presentation.hpp"
#include "sprout/launcher/setup_presentation.hpp"

#include <filesystem>

struct SDL_Renderer;

namespace sprout::launcher {

void render_launcher(SDL_Renderer* renderer, const LauncherState& state,
                     const std::filesystem::path& managed_image_root = {});
void render_setup(SDL_Renderer* renderer, const SetupPresentation& setup);
void render_profile_image_crop(SDL_Renderer* renderer,
                               const ProfileImageCropPresentation& crop);
void render_parent_pin(SDL_Renderer* renderer,
                       const ParentPinPresentation& pin);

}  // namespace sprout::launcher
