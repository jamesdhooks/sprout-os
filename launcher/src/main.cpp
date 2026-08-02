#include "desktop_view.hpp"
#include "sprout/launcher/launcher_state.hpp"

#include <SDL.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

using sprout::launcher::Action;
using sprout::launcher::EventType;
using sprout::launcher::LauncherEvent;
using sprout::launcher::LauncherState;

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
    case SDLK_SPACE:
    case SDLK_z:
      return Action::Confirm;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_x:
      return Action::Back;
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
    default:
      return std::nullopt;
  }
}

bool apply_action(LauncherState& state, Action action) {
  const auto event = state.handle(action);
  if (!event.has_value()) {
    return true;
  }

  if (event->type == EventType::ExitRequested) {
    return false;
  }

  if (event->type == EventType::MenuItemInvoked) {
    std::cout << "preview action: " << event->target << " (" << event->profile_id << ")\n";
  }
  return true;
}

SDL_Renderer* create_renderer(SDL_Window* window) {
  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  return renderer;
}

int run_smoke_test(SDL_Renderer* renderer) {
  LauncherState state(sprout::launcher::make_demo_household());
  sprout::launcher::render_launcher(renderer, state);
  (void)state.handle(Action::Confirm);
  sprout::launcher::render_launcher(renderer, state);
  (void)state.handle(Action::Back);
  (void)state.handle(Action::Right);
  (void)state.handle(Action::Confirm);
  sprout::launcher::render_launcher(renderer, state);
  return EXIT_SUCCESS;
}

int save_screenshot(SDL_Renderer* renderer, const char* path) {
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
  if (surface == nullptr) {
    std::cerr << "Screenshot surface creation failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  const int read_result =
      SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                           surface->pitch);
  const int save_result = read_result == 0 ? SDL_SaveBMP(surface, path) : -1;
  if (read_result != 0 || save_result != 0) {
    std::cerr << "Screenshot failed: " << SDL_GetError() << '\n';
    SDL_FreeSurface(surface);
    return EXIT_FAILURE;
  }

  SDL_FreeSurface(surface);
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* argv[]) {
  const bool smoke_test = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
  const bool screenshot = argc > 2 && std::string_view(argv[1]) == "--screenshot";

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  const std::uint32_t window_flags =
      smoke_test || screenshot ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
  SDL_Window* window = SDL_CreateWindow(
      "Sprout Launcher Preview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      window_flags);
  if (window == nullptr) {
    std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer* renderer = create_renderer(window);
  if (renderer == nullptr) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }
  SDL_RenderSetLogicalSize(renderer, 640, 480);

  if (smoke_test) {
    const int result = run_smoke_test(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  if (screenshot) {
    LauncherState state(sprout::launcher::make_demo_household());
    if (argc > 3 && std::string_view(argv[3]) == "child") {
      (void)state.handle(Action::Confirm);
    } else if (argc > 3 && std::string_view(argv[3]) == "parent") {
      (void)state.handle(Action::Right);
      (void)state.handle(Action::Confirm);
    }
    sprout::launcher::render_launcher(renderer, state);
    const int result = save_screenshot(renderer, argv[2]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
  }

  SDL_GameController* controller = nullptr;
  for (int index = 0; index < SDL_NumJoysticks(); ++index) {
    if (SDL_IsGameController(index)) {
      controller = SDL_GameControllerOpen(index);
      if (controller != nullptr) {
        break;
      }
    }
  }

  LauncherState state(sprout::launcher::make_demo_household());
  bool running = true;
  bool dirty = true;

  while (running) {
    SDL_Event event{};
    if (SDL_WaitEventTimeout(&event, 16) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        const auto action = keyboard_action(event.key.keysym.sym);
        if (action.has_value()) {
          running = apply_action(state, *action);
          dirty = true;
        }
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        const auto action = controller_action(event.cbutton.button);
        if (action.has_value()) {
          running = apply_action(state, *action);
          dirty = true;
        }
      }
    }

    if (dirty) {
      sprout::launcher::render_launcher(renderer, state);
      dirty = false;
    }
  }

  if (controller != nullptr) {
    SDL_GameControllerClose(controller);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return EXIT_SUCCESS;
}
