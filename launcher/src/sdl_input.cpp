#include "sprout/launcher/sdl_input.hpp"

namespace sprout::launcher {

std::optional<Action> keyboard_action(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP:
    case SDLK_w:
      return Action::Up;
    case SDLK_DOWN:
    case SDLK_s:
      return Action::Down;
    case SDLK_LEFT:
    case SDLK_a:
      return Action::Left;
    case SDLK_RIGHT:
    case SDLK_d:
      return Action::Right;
    case SDLK_RETURN:
      return Action::Menu;
    // The Miyoo's physical SELECT key reaches SDL as Escape (keyboard input,
    // not an SDL game-controller BACK event).
    case SDLK_ESCAPE:
    // Alternate Miyoo gpio-key translation used by this handheld revision.
    case SDLK_t:
    // Another Miyoo keymap reports physical SELECT as left Alt. It must take
    // the same session-level route rather than being treated as a dashboard
    // utility control.
    case SDLK_LALT:
      return Action::ProfileSelect;
    case SDLK_SPACE:
    case SDLK_z:
      return Action::Confirm;
    case SDLK_LCTRL:
      return Action::Back;
    case SDLK_LSHIFT:
      return Action::Filters;
    case SDLK_HOME:
      return Action::GameSwitcher;
    case SDLK_TAB:
    case SDLK_PAGEDOWN:
    case SDLK_RSHIFT:
      return Action::ZoomOut;
    case SDLK_BACKSPACE:
    case SDLK_PAGEUP:
    case SDLK_RCTRL:
      return Action::ZoomIn;
    case SDLK_x:
    case SDLK_b:
      return Action::Back;
    case SDLK_m:
      return Action::Menu;
    case SDLK_f:
      return Action::Filters;
    case SDLK_c:
      return Action::ClearFilters;
    case SDLK_e:
      return Action::ZoomIn;
    case SDLK_q:
      return Action::ZoomOut;
    default:
      return std::nullopt;
  }
}

std::optional<Action> controller_action(std::uint8_t button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return Action::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return Action::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return Action::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return Action::Right;
    case SDL_CONTROLLER_BUTTON_A:
      return Action::Confirm;
    case SDL_CONTROLLER_BUTTON_B:
      return Action::Back;
    case SDL_CONTROLLER_BUTTON_X:
      return Action::Filters;
    case SDL_CONTROLLER_BUTTON_Y:
      return Action::ClearFilters;
    case SDL_CONTROLLER_BUTTON_START:
      return Action::Menu;
    case SDL_CONTROLLER_BUTTON_BACK:
      return Action::ProfileSelect;
    case SDL_CONTROLLER_BUTTON_GUIDE:
      return Action::GameSwitcher;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      return Action::ZoomIn;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      return Action::ZoomOut;
    default:
      return std::nullopt;
  }
}

}  // namespace sprout::launcher
