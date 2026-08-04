#include "sprout/runtime/package.hpp"
#include "sprout/runtime/session.hpp"
#include "sprout/ui/font_metrics.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cmath>
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
  std::filesystem::path capture_title;
  std::string capture_state;
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
    } else if (argument == "--capture-title" && index + 1 < count) {
      arguments.capture_title = values[++index];
    } else if (argument == "--capture-state" && index + 1 < count) {
      arguments.capture_state = values[++index];
    } else if (argument == "--smoke-test") {
      arguments.smoke_test = true;
    } else {
      throw std::runtime_error("Usage: sprout-runtime --package PATH "
                               "[--storage PATH] [--seed NUMBER] [--capture BMP] "
                               "[--capture-title BMP] "
                               "[--capture-state NAME] "
                               "[--smoke-test]");
    }
  }
  if (arguments.package.empty()) {
    throw std::runtime_error("A native-game package path is required");
  }
  if (!arguments.capture_state.empty() && arguments.capture.empty()) {
    throw std::runtime_error("--capture-state requires --capture");
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
                   keyboard[SDL_SCANCODE_H] != 0 ||
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
    SDL_SetTextureScaleMode(textures_[index], SDL_ScaleModeLinear);
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

void draw_text(SDL_Renderer* renderer, const std::string& text,
               const SDL_Rect& region, SDL_Color color);

void draw_frame(SDL_Renderer* renderer, sprout::runtime::Session& session,
                TextureStore& textures, GeometryBuffers& geometry) {
  SDL_SetRenderDrawColor(renderer, 20, 24, 28, 255);
  SDL_RenderClear(renderer);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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
    if (command.type == sprout::runtime::DrawCommandType::Circle) {
      const auto& circle = command.circle;
      SDL_SetRenderDrawColor(renderer, circle.red, circle.green, circle.blue,
                             circle.alpha);
      for (int offset_y = -circle.radius; offset_y <= circle.radius;
           ++offset_y) {
        const int half_width = static_cast<int>(std::floor(std::sqrt(
            static_cast<double>(circle.radius * circle.radius -
                                offset_y * offset_y))));
        SDL_Rect scanline{circle.x - half_width, circle.y + offset_y,
                          half_width * 2 + 1, 1};
        SDL_RenderFillRect(renderer, &scanline);
      }
      ++command_index;
      continue;
    }
    if (command.type == sprout::runtime::DrawCommandType::Label) {
      const auto& label = command.label;
      draw_text(renderer, label.text,
                {label.x, label.y, label.width, label.height},
                {label.red, label.green, label.blue, label.alpha});
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

const char* glyph(char character) {
  switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
    case 'A': return "01110100011000111111100011000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01111100001000010000100001000001111";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01111100001000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "11111001000010000100001000010011111";
    case 'J': return "00111000100001000010000101001001100";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001110011010110011100011000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'Q': return "01110100011000110001101011001001101";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101000100";
    case 'W': return "10001100011000110101101011101110001";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case 'Z': return "11111000010001000100010001000011111";
    case '&': return "01100100100100001000101011001001101";
    default: return "00000000000000000000000000000000000";
  }
}

SDL_Rect logical_region(const sprout::runtime::NormalizedRegion& region,
                        int width, int height) {
  return {region.x * width / 1000, region.y * height / 1000,
          region.width * width / 1000, region.height * height / 1000};
}

void draw_text(SDL_Renderer* renderer, const std::string& text,
               const SDL_Rect& region, SDL_Color color) {
  static SDL_Renderer* font_renderer = nullptr;
  static SDL_Texture* font_texture = nullptr;
  if (font_renderer != renderer) {
    if (font_texture != nullptr) SDL_DestroyTexture(font_texture);
    char* base = SDL_GetBasePath();
    const std::filesystem::path path =
        base == nullptr ? std::filesystem::path{}
                        : std::filesystem::path(base) / "assets" / "fonts" /
                              "nunito-semibold.png";
    if (base != nullptr) SDL_free(base);
    font_texture = path.empty() ? nullptr
                                : IMG_LoadTexture(renderer, path.string().c_str());
    if (font_texture != nullptr) {
      SDL_SetTextureBlendMode(font_texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
      SDL_SetTextureScaleMode(font_texture, SDL_ScaleModeLinear);
#endif
    }
    font_renderer = renderer;
  }
  if (font_texture != nullptr) {
    const auto metric_for = [](char character) -> const sprout::ui::UiGlyphMetric& {
      const unsigned char value = static_cast<unsigned char>(character);
      const int codepoint = value >= sprout::ui::kUiFontFirstCodepoint &&
                                    value <= sprout::ui::kUiFontLastCodepoint
                                ? value
                                : '?';
      return sprout::ui::kUiFontRegularMetrics[
          static_cast<std::size_t>(codepoint - sprout::ui::kUiFontFirstCodepoint)];
    };
    double source_width = 0.0;
    for (const char character : text) {
      source_width += metric_for(character).advance;
    }
    const int preferred_height = std::max(1, std::min(18, region.h - 8));
    const double height_ratio =
        static_cast<double>(preferred_height) / sprout::ui::kUiFontSourceSize;
    const double width_ratio = source_width > 0.0
                                   ? static_cast<double>(region.w) / source_width
                                   : height_ratio;
    const double ratio = std::min(height_ratio, width_ratio);
    const int height = std::max(
        1, static_cast<int>(std::round(sprout::ui::kUiFontSourceSize * ratio)));
    int width = 0;
    for (const char character : text) {
      width += static_cast<int>(std::round(metric_for(character).advance * ratio));
    }
    int x = region.x + (region.w - width) / 2;
    const int baseline = region.y + (region.h - height) / 2;
    const bool had_clip = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    SDL_Rect previous_clip{};
    if (had_clip) SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &region);
    SDL_SetTextureColorMod(font_texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font_texture, color.a);
    for (const char character : text) {
      const auto& metric = metric_for(character);
      if (metric.width > 0 && metric.height > 0) {
        const SDL_Rect source{metric.source_x, metric.source_y, metric.width,
                              metric.height};
        const SDL_Rect destination{
            x + static_cast<int>(std::round(metric.bearing_x * ratio)),
            baseline + static_cast<int>(std::round(
                           (sprout::ui::kUiFontRegularAscent + metric.bearing_top) *
                           ratio)),
            std::max(1, static_cast<int>(std::round(metric.width * ratio))),
            std::max(1, static_cast<int>(std::round(metric.height * ratio)))};
        SDL_RenderCopy(renderer, font_texture, &source, &destination);
      }
      x += static_cast<int>(std::round(metric.advance * ratio));
    }
    SDL_RenderSetClipRect(renderer, had_clip ? &previous_clip : nullptr);
    return;
  }
  const int units = std::max(1, static_cast<int>(text.size()) * 6 - 1);
  const int scale = std::max(1, std::min(region.w / units, region.h / 7));
  const int width = units * scale;
  const int height = 7 * scale;
  const int origin_x = region.x + (region.w - width) / 2;
  const int origin_y = region.y + (region.h - height) / 2;
  const auto pass = [&](int offset, SDL_Color pass_color) {
    SDL_SetRenderDrawColor(renderer, pass_color.r, pass_color.g, pass_color.b,
                           pass_color.a);
    for (std::size_t letter = 0; letter < text.size(); ++letter) {
      const char* pixels = glyph(text[letter]);
      for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
          if (pixels[row * 5 + column] != '1') continue;
          SDL_Rect pixel{origin_x + static_cast<int>(letter) * 6 * scale +
                             column * scale + offset,
                         origin_y + row * scale + offset, scale, scale};
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
    }
  };
  pass(std::max(1, scale / 2), {12, 24, 19, 210});
  pass(0, color);
}

void fill_rounded_rect(SDL_Renderer* renderer, const SDL_Rect& rectangle,
                       int radius, SDL_Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_Rect middle{rectangle.x + radius, rectangle.y,
                  rectangle.w - radius * 2, rectangle.h};
  SDL_Rect center{rectangle.x, rectangle.y + radius,
                  rectangle.w, rectangle.h - radius * 2};
  SDL_RenderFillRect(renderer, &middle);
  SDL_RenderFillRect(renderer, &center);
  for (int y = 0; y < radius; ++y) {
    const int inset = radius - static_cast<int>(
        std::sqrt(static_cast<double>(radius * radius - (radius - y) * (radius - y))));
    SDL_Rect top{rectangle.x + inset, rectangle.y + y,
                 rectangle.w - inset * 2, 1};
    SDL_Rect bottom{rectangle.x + inset, rectangle.y + rectangle.h - 1 - y,
                    rectangle.w - inset * 2, 1};
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bottom);
  }
}

void draw_title_screen(SDL_Renderer* renderer,
                       const sprout::runtime::PackageManifest& package,
                       SDL_Texture* texture) {
  SDL_SetRenderDrawColor(renderer, 16, 32, 25, 255);
  SDL_RenderClear(renderer);
  SDL_Rect destination{0, 0, package.logical_width, package.logical_height};
  SDL_Rect source{0, 0, package.title_screen.image_width,
                  package.title_screen.image_height};
  if (package.title_screen.fit == sprout::runtime::PresentationFit::Cover) {
    const long long source_ratio = static_cast<long long>(source.w) * destination.h;
    const long long target_ratio = static_cast<long long>(destination.w) * source.h;
    if (source_ratio > target_ratio) {
      const int cropped = source.h * destination.w / destination.h;
      source.x = (source.w - cropped) / 2;
      source.w = cropped;
    } else if (source_ratio < target_ratio) {
      const int cropped = source.w * destination.h / destination.w;
      source.y = (source.h - cropped) / 2;
      source.h = cropped;
    }
  } else {
    const double scale = std::min(static_cast<double>(destination.w) / source.w,
                                  static_cast<double>(destination.h) / source.h);
    destination.w = static_cast<int>(source.w * scale);
    destination.h = static_cast<int>(source.h * scale);
    destination.x = (package.logical_width - destination.w) / 2;
    destination.y = (package.logical_height - destination.h) / 2;
  }
  SDL_RenderCopy(renderer, texture, &source, &destination);

  const SDL_Rect controls = logical_region(package.title_screen.controls_region,
                                           package.logical_width,
                                           package.logical_height);
  const auto& background = package.title_screen.controls_background;
  const auto& foreground = package.title_screen.controls_foreground;
  fill_rounded_rect(renderer, controls, std::min(controls.h / 2, 12),
                    {background[0], background[1], background[2], background[3]});
  draw_text(renderer, "A Start    B Back", controls,
            {foreground[0], foreground[1], foreground[2], foreground[3]});
  SDL_RenderPresent(renderer);
}

SDL_Texture* load_title_texture(SDL_Renderer* renderer,
                                const sprout::runtime::TitleScreen& title) {
  SDL_Texture* texture = IMG_LoadTexture(renderer, title.image.string().c_str());
  if (texture == nullptr) {
    throw std::runtime_error(std::string("Could not load title screen: ") +
                             IMG_GetError());
  }
  int width = 0;
  int height = 0;
  if (SDL_QueryTexture(texture, nullptr, nullptr, &width, &height) != 0 ||
      width != title.image_width || height != title.image_height) {
    SDL_DestroyTexture(texture);
    throw std::runtime_error("Title screen dimensions do not match manifest");
  }
#if SDL_VERSION_ATLEAST(2, 0, 12)
  SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif
  return texture;
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

    if (arguments.smoke_test) {
      session.start();
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
    const auto window_flags = arguments.capture.empty() &&
                                      arguments.capture_title.empty()
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
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
      if (SDL_IsGameController(index)) {
        controller = SDL_GameControllerOpen(index);
        if (controller != nullptr) break;
      }
    }

    SDL_Texture* title_texture = nullptr;
    if (package.title_screen.enabled) {
      title_texture = load_title_texture(renderer, package.title_screen);
      draw_title_screen(renderer, package, title_texture);
      if (!arguments.capture_title.empty()) {
        capture_frame(renderer, arguments.capture_title);
        SDL_DestroyTexture(title_texture);
        if (controller != nullptr) SDL_GameControllerClose(controller);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 0;
      }
      bool waiting = arguments.capture.empty();
      bool primary_was_down = false;
      while (waiting) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
          if (event.type == SDL_QUIT) waiting = false;
        }
        const auto actions = read_actions(controller);
        if (actions.back || actions.secondary) {
          SDL_DestroyTexture(title_texture);
          if (controller != nullptr) SDL_GameControllerClose(controller);
          SDL_DestroyRenderer(renderer);
          SDL_DestroyWindow(window);
          IMG_Quit();
          SDL_Quit();
          return 0;
        }
        if (actions.primary && !primary_was_down) waiting = false;
        primary_was_down = actions.primary;
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
      }
      SDL_DestroyTexture(title_texture);
    } else if (!arguments.capture_title.empty()) {
      throw std::runtime_error("Package does not define a title screen");
    }

    session.start();
    if (!arguments.capture_state.empty()) {
      session.apply_capture_scenario(arguments.capture_state);
    }
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
