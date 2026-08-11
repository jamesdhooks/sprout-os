#include "sprout/launcher/sdl_input.hpp"

#include <SDL.h>

#include <iostream>

int main(int, char**) {
  const auto start = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_START);
  if (start != sprout::launcher::Action::Confirm) {
    std::cerr << "Physical Start must activate the selected library title\n";
    return 1;
  }

  const auto a = sprout::launcher::controller_action(SDL_CONTROLLER_BUTTON_A);
  if (a != sprout::launcher::Action::Confirm) {
    std::cerr << "Controller A must continue to confirm selections\n";
    return 1;
  }

  const auto guide = sprout::launcher::controller_action(
      SDL_CONTROLLER_BUTTON_GUIDE);
  if (guide != sprout::launcher::Action::SystemMenu) {
    std::cerr << "Guide must retain the contained system-menu action\n";
    return 1;
  }

  return 0;
}
