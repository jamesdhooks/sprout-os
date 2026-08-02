#pragma once

#include "sprout/launcher/launcher_state.hpp"

struct SDL_Renderer;

namespace sprout::launcher {

void render_launcher(SDL_Renderer* renderer, const LauncherState& state);

}  // namespace sprout::launcher
