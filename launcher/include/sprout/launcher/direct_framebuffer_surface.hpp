#pragma once

struct SDL_Surface;

namespace sprout::launcher {

// Returns the software surface used by the Miyoo direct-framebuffer renderer,
// or nullptr when normal SDL presentation is active.
SDL_Surface* direct_framebuffer_surface() noexcept;

}  // namespace sprout::launcher
