#include "sprout/runtime/package.hpp"
#include "sprout/runtime/session.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Arguments {
  std::filesystem::path package;
  std::filesystem::path storage{"sprout-data/native-games"};
  std::filesystem::path capture;
  std::uint64_t seed{1};
  bool smoke_test{};
};

Arguments parse_arguments(int count, char** values) {
  Arguments arguments;
  for (int index = 1; index < count; ++index) {
    const std::string argument = values[index];
    if (argument == "--package" && index + 1 < count) {
      arguments.package = values[++index];
    } else if (argument == "--storage" && index + 1 < count) {
      arguments.storage = values[++index];
    } else if (argument == "--seed" && index + 1 < count) {
      arguments.seed = std::stoull(values[++index]);
    } else if (argument == "--capture" && index + 1 < count) {
      arguments.capture = values[++index];
    } else if (argument == "--smoke-test") {
      arguments.smoke_test = true;
    } else {
      throw std::runtime_error("Usage: sprout-runtime --package PATH "
                               "[--storage PATH] [--seed NUMBER] [--capture BMP] "
                               "[--smoke-test]");
    }
  }
  if (arguments.package.empty()) {
    throw std::runtime_error("A native-game package path is required");
  }
  return arguments;
}

bool key_down(const std::uint8_t* keyboard, SDL_Scancode first,
              SDL_Scancode second) {
  return keyboard[first] != 0 || keyboard[second] != 0;
}

sprout::runtime::Actions read_actions(SDL_GameController* controller) {
  const std::uint8_t* keyboard = SDL_GetKeyboardState(nullptr);
  const auto button = [controller](SDL_GameControllerButton value) {
    return controller != nullptr && SDL_GameControllerGetButton(controller, value) != 0;
  };
  return {
      .up = key_down(keyboard, SDL_SCANCODE_UP, SDL_SCANCODE_W) ||
            button(SDL_CONTROLLER_BUTTON_DPAD_UP),
      .down = key_down(keyboard, SDL_SCANCODE_DOWN, SDL_SCANCODE_S) ||
              button(SDL_CONTROLLER_BUTTON_DPAD_DOWN),
      .left = key_down(keyboard, SDL_SCANCODE_LEFT, SDL_SCANCODE_A) ||
              button(SDL_CONTROLLER_BUTTON_DPAD_LEFT),
      .right = key_down(keyboard, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D) ||
               button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT),
      .primary = key_down(keyboard, SDL_SCANCODE_Z, SDL_SCANCODE_RETURN) ||
                 button(SDL_CONTROLLER_BUTTON_A),
      .secondary = key_down(keyboard, SDL_SCANCODE_X, SDL_SCANCODE_SPACE) ||
                   button(SDL_CONTROLLER_BUTTON_B),
      .start = keyboard[SDL_SCANCODE_RETURN] != 0 ||
               button(SDL_CONTROLLER_BUTTON_START),
      .back = keyboard[SDL_SCANCODE_ESCAPE] != 0 ||
              button(SDL_CONTROLLER_BUTTON_BACK),
  };
}

void print_events(sprout::runtime::Session& session) {
  for (const auto& event : session.drain_events()) {
    std::cout << "SPROUT_EVENT\t" << event.type << '\t' << event.value << '\n';
  }
}

class TextureStore {
 public:
  TextureStore(SDL_Renderer* renderer,
               const sprout::runtime::AssetCatalogue& assets)
      : renderer_(renderer), assets_(assets), textures_(assets.atlases.size()) {}

  ~TextureStore() {
    reset();
  }

  TextureStore(const TextureStore&) = delete;
  TextureStore& operator=(const TextureStore&) = delete;

  void reset() {
    for (SDL_Texture*& texture : textures_) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
  }

  SDL_Texture* get(std::size_t index) {
    if (index >= textures_.size()) {
      throw std::runtime_error("Draw command references an unknown atlas");
    }
    if (textures_[index] != nullptr) return textures_[index];
    const auto& atlas = assets_.atlases[index];
    textures_[index] = IMG_LoadTexture(renderer_, atlas.image.string().c_str());
    if (textures_[index] == nullptr) {
      throw std::runtime_error("Could not load sprite atlas " + atlas.id + ": " +
                               IMG_GetError());
    }
    int width = 0;
    int height = 0;
    if (SDL_QueryTexture(textures_[index], nullptr, nullptr, &width, &height) != 0 ||
        width != atlas.width || height != atlas.height) {
      throw std::runtime_error("Sprite atlas dimensions do not match manifest: " +
                               atlas.id);
    }
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(textures_[index], SDL_ScaleModeNearest);
#endif
    SDL_SetTextureBlendMode(textures_[index], SDL_BLENDMODE_BLEND);
    return textures_[index];
  }

  const sprout::runtime::TextureAtlas& atlas(std::size_t index) const {
    if (index >= assets_.atlases.size()) {
      throw std::runtime_error("Draw command references an unknown atlas");
    }
    return assets_.atlases[index];
  }

 private:
  SDL_Renderer* renderer_{};
  const sprout::runtime::AssetCatalogue& assets_;
  std::vector<SDL_Texture*> textures_;
};

struct GeometryBuffers {
  GeometryBuffers() {
    vertices.reserve(4096 * 4);
    indices.reserve(4096 * 6);
  }

  std::vector<SDL_Vertex> vertices;
  std::vector<int> indices;
};

void draw_frame(SDL_Renderer* renderer, sprout::runtime::Session& session,
                TextureStore& textures, GeometryBuffers& geometry) {
  SDL_SetRenderDrawColor(renderer, 20, 24, 28, 255);
  SDL_RenderClear(renderer);
  const auto& commands = session.render();
  std::size_t command_index = 0;
  while (command_index < commands.size()) {
    const auto& command = commands[command_index];
    if (command.type == sprout::runtime::DrawCommandType::Rectangle) {
      const auto& rectangle = command.rectangle;
      SDL_Rect target{rectangle.x, rectangle.y, rectangle.width, rectangle.height};
      SDL_SetRenderDrawColor(renderer, rectangle.red, rectangle.green,
                             rectangle.blue, rectangle.alpha);
      SDL_RenderFillRect(renderer, &target);
      ++command_index;
      continue;
    }

    const std::size_t atlas_index = command.sprite.atlas;
    SDL_Texture* texture = textures.get(atlas_index);
    const auto& atlas = textures.atlas(atlas_index);
    geometry.vertices.clear();
    geometry.indices.clear();
    while (command_index < commands.size() &&
           commands[command_index].type ==
               sprout::runtime::DrawCommandType::Sprite &&
           commands[command_index].sprite.atlas == atlas_index) {
      const auto& sprite = commands[command_index].sprite;
      float u0 = static_cast<float>(sprite.source_x) / atlas.width;
      float v0 = static_cast<float>(sprite.source_y) / atlas.height;
      float u1 = static_cast<float>(sprite.source_x + sprite.source_width) /
                 atlas.width;
      float v1 = static_cast<float>(sprite.source_y + sprite.source_height) /
                 atlas.height;
      if (sprite.flip_x) std::swap(u0, u1);
      if (sprite.flip_y) std::swap(v0, v1);
      const float x0 = static_cast<float>(sprite.x);
      const float y0 = static_cast<float>(sprite.y);
      const float x1 = static_cast<float>(sprite.x + sprite.width);
      const float y1 = static_cast<float>(sprite.y + sprite.height);
      const SDL_Color color{sprite.red, sprite.green, sprite.blue, sprite.alpha};
      const int base = static_cast<int>(geometry.vertices.size());
      geometry.vertices.push_back({{x0, y0}, color, {u0, v0}});
      geometry.vertices.push_back({{x1, y0}, color, {u1, v0}});
      geometry.vertices.push_back({{x1, y1}, color, {u1, v1}});
      geometry.vertices.push_back({{x0, y1}, color, {u0, v1}});
      geometry.indices.insert(geometry.indices.end(),
                              {base, base + 1, base + 2,
                               base, base + 2, base + 3});
      ++command_index;
    }
    if (SDL_RenderGeometry(renderer, texture, geometry.vertices.data(),
                           static_cast<int>(geometry.vertices.size()),
                           geometry.indices.data(),
                           static_cast<int>(geometry.indices.size())) != 0) {
      throw std::runtime_error(std::string("Could not draw sprite batch: ") +
                               SDL_GetError());
    }
  }
  SDL_RenderPresent(renderer);
}

void capture_frame(SDL_Renderer* renderer, const std::filesystem::path& path) {
  int width = 0;
  int height = 0;
  if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
    throw std::runtime_error(std::string("Could not inspect runtime output: ") +
                             SDL_GetError());
  }
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  if (surface == nullptr ||
      SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                           surface->pixels, surface->pitch) != 0) {
    SDL_FreeSurface(surface);
    throw std::runtime_error(std::string("Could not capture runtime frame: ") +
                             SDL_GetError());
  }
  std::filesystem::create_directories(path.parent_path());
  if (SDL_SaveBMP(surface, path.string().c_str()) != 0) {
    SDL_FreeSurface(surface);
    throw std::runtime_error(std::string("Could not save runtime frame: ") +
                             SDL_GetError());
  }
  SDL_FreeSurface(surface);
}

}  // namespace

int main(int count, char** values) {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_GameController* controller = nullptr;
  try {
    const Arguments arguments = parse_arguments(count, values);
    auto package = sprout::runtime::load_package(arguments.package);
    sprout::runtime::Session session(package, arguments.storage, arguments.seed);
    session.start();

    if (arguments.smoke_test) {
      session.step({});
      session.render();
      session.stop();
      print_events(session);
      return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
      throw std::runtime_error(std::string("SDL initialization failed: ") +
                               SDL_GetError());
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
      throw std::runtime_error(std::string("PNG decoder initialization failed: ") +
                               IMG_GetError());
    }
    const auto window_flags = arguments.capture.empty()
                                  ? SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
                                  : SDL_WINDOW_HIDDEN;
    window = SDL_CreateWindow(package.title.c_str(), SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 640, 480, window_flags);
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (window != nullptr && renderer == nullptr) {
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (window == nullptr || renderer == nullptr) {
      throw std::runtime_error(std::string("Could not create runtime window: ") +
                               SDL_GetError());
    }
    SDL_RenderSetLogicalSize(renderer, package.logical_width,
                             package.logical_height);
    TextureStore textures(renderer, package.assets);
    GeometryBuffers geometry;
    draw_frame(renderer, session, textures, geometry);
    if (!arguments.capture.empty()) {
      capture_frame(renderer, arguments.capture);
      session.stop();
      print_events(session);
      textures.reset();
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      IMG_Quit();
      SDL_Quit();
      return 0;
    }
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
      if (SDL_IsGameController(index)) {
        controller = SDL_GameControllerOpen(index);
        if (controller != nullptr) {
          break;
        }
      }
    }

    constexpr auto tick = std::chrono::microseconds(16667);
    auto previous = std::chrono::steady_clock::now();
    auto accumulator = std::chrono::steady_clock::duration::zero();
    bool running = true;
    while (running) {
      SDL_Event event{};
      while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
          running = false;
        }
      }

      const auto now = std::chrono::steady_clock::now();
      accumulator += now - previous;
      previous = now;
      int steps = 0;
      while (accumulator >= tick && steps < 5 && running) {
        const auto actions = read_actions(controller);
        if (actions.back) {
          running = false;
          break;
        }
        session.step(actions);
        accumulator -= tick;
        ++steps;
      }
      if (steps == 5) {
        accumulator = std::chrono::steady_clock::duration::zero();
      }

      draw_frame(renderer, session, textures, geometry);
      print_events(session);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    session.stop();
    print_events(session);
    if (controller != nullptr) SDL_GameControllerClose(controller);
    textures.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
  } catch (const std::exception& error) {
    if (controller != nullptr) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    std::cerr << "sprout-runtime: " << error.what() << '\n';
    return 1;
  }
}
