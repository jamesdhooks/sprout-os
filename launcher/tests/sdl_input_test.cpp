#include "sprout/launcher/sdl_input.hpp"

#include <SDL.h>

#include <iostream>

int main(int, char**) {
  const auto start = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_START);
  if (start != sprout::launcher::Action::Menu) {
    std::cerr << "Physical Start must open the primary Sprout menu\n";
    return 1;
  }

  const auto a = sprout::launcher::controller_action(SDL_CONTROLLER_BUTTON_A);
  if (a != sprout::launcher::Action::Confirm) {
    std::cerr << "Controller A must continue to confirm selections\n";
    return 1;
  }

  const auto guide = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_GUIDE);
  if (guide != sprout::launcher::Action::GameSwitcher) {
    std::cerr << "Guide must request Onion GameSwitcher\n";
    return 1;
  }

  const auto filters = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_X);
  if (filters != sprout::launcher::Action::Filters) {
    std::cerr << "Controller X must open the game filters\n";
    return 1;
  }

  const auto clear_filters = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_Y);
  if (clear_filters != sprout::launcher::Action::ClearFilters) {
    std::cerr << "Controller Y must clear active game filters\n";
    return 1;
  }

  const auto require_key = [](SDL_Keycode key, sprout::launcher::Action expected,
                              const char* message) {
    if (sprout::launcher::keyboard_action(key) != expected) {
      std::cerr << message << '\n';
      return false;
    }
    return true;
  };
  if (!require_key(SDLK_SPACE, sprout::launcher::Action::Confirm,
                   "Miyoo A must confirm") ||
      !require_key(SDLK_LCTRL, sprout::launcher::Action::Back,
                   "Miyoo B must go back") ||
      !require_key(SDLK_LSHIFT, sprout::launcher::Action::Filters,
                   "Miyoo X must open filters") ||
      !require_key(SDLK_LALT, sprout::launcher::Action::ClearFilters,
                   "Miyoo Y must clear filters") ||
      !require_key(SDLK_RETURN, sprout::launcher::Action::Menu,
                   "Miyoo Start must open the primary menu") ||
      !require_key(SDLK_ESCAPE, sprout::launcher::Action::ProfileSelect,
                   "Miyoo Select must return to profile selection") ||
      !require_key(SDLK_HOME, sprout::launcher::Action::GameSwitcher,
                   "Miyoo Menu must open Onion GameSwitcher") ||
      !require_key(SDLK_TAB, sprout::launcher::Action::ZoomOut,
                   "Miyoo L must not act as Back") ||
      !require_key(SDLK_BACKSPACE, sprout::launcher::Action::ZoomIn,
                   "Miyoo R must not act as Back")) {
    return 1;
  }
  if (sprout::launcher::keyboard_action(SDLK_ESCAPE).has_value()) {
    std::cerr << "Miyoo Select must not expose a second Sprout menu\n";
    return 1;
  }

  return 0;
}
