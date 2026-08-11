#pragma once

#include "sprout/launcher/launcher_state.hpp"

#include <SDL.h>

#include <cstdint>
#include <optional>

namespace sprout::launcher {

std::optional<Action> keyboard_action(SDL_Keycode key);
std::optional<Action> controller_action(std::uint8_t button);

}  // namespace sprout::launcher
