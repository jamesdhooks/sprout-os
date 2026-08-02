#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/setup_presentation.hpp"

struct SDL_Renderer;

namespace sprout::launcher {

void render_launcher(SDL_Renderer* renderer, const LauncherState& state);
void render_setup(SDL_Renderer* renderer, const SetupPresentation& setup);

}  // namespace sprout::launcher
