#include "desktop_view.hpp"
#include "sprout/launcher/direct_framebuffer_surface.hpp"
#include "sprout/launcher/built_in_avatar.hpp"
#include "sprout/launcher/framebuffer_page_writer.hpp"
#include "sprout/launcher/portrait_outline.hpp"
#include "sprout/launcher/profile_image_importer.hpp"
#include "sprout/launcher/profile_select_layout.hpp"
#include "sprout/launcher/string_compat.hpp"
#include "sprout/ui/font_metrics.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sprout::launcher {
namespace {

using sprout::ui::UiGlyphMetric;
using sprout::ui::kUiFontFirstCodepoint;
using sprout::ui::kUiFontHeadingAscent;
using sprout::ui::kUiFontHeadingMetrics;
using sprout::ui::kUiFontLastCodepoint;
using sprout::ui::kUiFontRegularAscent;
using sprout::ui::kUiFontRegularMetrics;
using sprout::ui::kUiFontSourceSize;

constexpr int kWidth = 640;
constexpr int kHeight = 480;

void present_frame(SDL_Renderer* renderer) {
  const char* framebuffer_path = std::getenv("SPROUT_DIRECT_FRAMEBUFFER");
  if (framebuffer_path == nullptr || framebuffer_path[0] == '\0') {
    SDL_RenderPresent(renderer);
    return;
  }

  SDL_Surface* surface = direct_framebuffer_surface();
  if (surface == nullptr || surface->pixels == nullptr || surface->w <= 0 ||
      surface->h <= 0 || surface->pitch < surface->w * 4) {
    std::cerr << "SPROUT_FRAMEBUFFER direct-surface-unavailable" << std::endl;
    return;
  }

  constexpr std::size_t kBytesPerPixel = 4;
  const int width = surface->w;
  const int height = surface->h;
  std::vector<std::uint8_t> frame(static_cast<std::size_t>(width) *
                                  static_cast<std::size_t>(height) *
                                  kBytesPerPixel);
  if (SDL_LockSurface(surface) != 0) {
    std::cerr << "SPROUT_FRAMEBUFFER surface-lock-failed error=" << SDL_GetError()
              << std::endl;
    return;
  }
  const auto* pixels = static_cast<const std::uint8_t*>(surface->pixels);
  for (int destination_y = 0; destination_y < height; ++destination_y) {
    const int source_y = height - 1 - destination_y;
    for (int destination_x = 0; destination_x < width; ++destination_x) {
      const int source_x = width - 1 - destination_x;
      const auto* source = pixels + source_y * surface->pitch +
                           source_x * static_cast<int>(kBytesPerPixel);
      auto* destination = frame.data() +
                          (static_cast<std::size_t>(destination_y) * width +
                           static_cast<std::size_t>(destination_x)) *
                              kBytesPerPixel;
      std::copy_n(source, kBytesPerPixel, destination);
    }
  }
  SDL_UnlockSurface(surface);

  // SDL's mmiyoo software-renderer readback is black on device. The surface
  // above is the authoritative rendered frame; mirror it to both fb0 pages.
  SDL_RenderPresent(renderer);
  if (!write_framebuffer_pages(framebuffer_path, frame.data(), frame.size(), 2)) {
    std::cerr << "SPROUT_FRAMEBUFFER write-failed path=" << framebuffer_path
              << std::endl;
    return;
  }

  static bool first_copy_logged = false;
  if (!first_copy_logged) {
    std::cerr << "SPROUT_FRAMEBUFFER surface-copy-complete size=" << width << 'x'
              << height << " format=RGB888 pages=2" << std::endl;
    first_copy_logged = true;
  }
}

struct Color {
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::uint8_t alpha{255};
};

constexpr Color kBackground{224, 239, 215};
constexpr Color kPanel{255, 250, 231, 224};
constexpr Color kPanelFocused{255, 244, 196, 244};
constexpr Color kText{37, 67, 53};
constexpr Color kMuted{83, 108, 91};
constexpr Color kFocus{229, 117, 87};
constexpr Color kHoney{241, 188, 73};

void set_color(SDL_Renderer* renderer, Color color) {
  SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

void fill_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  set_color(renderer, color);
  const int radius = std::min({14, rect.w / 3, rect.h / 3});
  if (radius < 3 || rect.w >= kWidth || rect.h >= kHeight) {
    SDL_RenderFillRect(renderer, &rect);
    return;
  }
  SDL_Rect middle{rect.x + radius, rect.y, rect.w - radius * 2, rect.h};
  SDL_Rect center{rect.x, rect.y + radius, rect.w, rect.h - radius * 2};
  SDL_RenderFillRect(renderer, &middle);
  SDL_RenderFillRect(renderer, &center);
  for (int offset = 0; offset < radius; ++offset) {
    const int vertical = radius - offset;
    const int inset = radius - static_cast<int>(
        std::sqrt(static_cast<double>(radius * radius - vertical * vertical)));
    SDL_RenderDrawLine(renderer, rect.x + inset, rect.y + offset,
                       rect.x + rect.w - inset - 1, rect.y + offset);
    SDL_RenderDrawLine(renderer, rect.x + inset,
                       rect.y + rect.h - offset - 1,
                       rect.x + rect.w - inset - 1,
                       rect.y + rect.h - offset - 1);
  }
}

void outline_rect(SDL_Renderer* renderer, SDL_Rect rect, int thickness, Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  set_color(renderer, color);
  for (int index = 0; index < thickness; ++index) {
    const int radius = std::min({14, rect.w / 3, rect.h / 3});
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y,
                       rect.x + rect.w - radius - 1, rect.y);
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y + rect.h - 1,
                       rect.x + rect.w - radius - 1, rect.y + rect.h - 1);
    SDL_RenderDrawLine(renderer, rect.x, rect.y + radius,
                       rect.x, rect.y + rect.h - radius - 1);
    SDL_RenderDrawLine(renderer, rect.x + rect.w - 1, rect.y + radius,
                       rect.x + rect.w - 1, rect.y + rect.h - radius - 1);
    for (int angle = 0; angle <= 90; ++angle) {
      const double radians = static_cast<double>(angle) * 3.141592653589793 / 180.0;
      const int dx = static_cast<int>(std::round(std::cos(radians) * radius));
      const int dy = static_cast<int>(std::round(std::sin(radians) * radius));
      SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
      SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius - 1 + dx,
                          rect.y + radius - dy);
      SDL_RenderDrawPoint(renderer, rect.x + radius - dx,
                          rect.y + rect.h - radius - 1 + dy);
      SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius - 1 + dx,
                          rect.y + rect.h - radius - 1 + dy);
    }
    ++rect.x;
    ++rect.y;
    rect.w -= 2;
    rect.h -= 2;
  }
}

std::array<std::uint8_t, 7> glyph(char raw_character) {
  const char character = static_cast<char>(std::toupper(static_cast<unsigned char>(raw_character)));
  switch (character) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case '?': return {14, 17, 1, 2, 4, 0, 4};
    case ',': return {0, 0, 0, 0, 0, 4, 8};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case '>': return {16, 8, 4, 2, 4, 8, 16};
    case '*': return {0, 21, 14, 31, 14, 21, 0};
    case '&': return {12, 18, 20, 8, 21, 18, 13};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

std::filesystem::path executable_asset(std::string_view relative) {
  char* base = SDL_GetBasePath();
  if (base == nullptr) return {};
  const std::filesystem::path root(base);
  SDL_free(base);
  return root / "assets" / std::filesystem::path(relative);
}

std::string path_as_utf8(const std::filesystem::path& path);

SDL_Texture* cached_texture(SDL_Renderer* renderer,
                            const std::filesystem::path& path) {
  struct Entry {
    SDL_Renderer* renderer;
    std::string path;
    SDL_Texture* texture;
  };
  static std::vector<Entry> cache;
  const auto encoded = path_as_utf8(path);
  const auto found = std::find_if(cache.begin(), cache.end(), [&](const Entry& entry) {
    return entry.renderer == renderer && entry.path == encoded;
  });
  if (found != cache.end()) return found->texture;
  SDL_Texture* texture = IMG_LoadTexture(renderer, encoded.c_str());
  if (texture == nullptr) {
    std::cerr << "SPROUT_TEXTURE load-failed path=" << encoded
              << " error=" << IMG_GetError() << '\n';
    return nullptr;
  }
#if SDL_VERSION_ATLEAST(2, 0, 12)
  SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  cache.push_back({renderer, encoded, texture});
  return texture;
}

int font_height(int scale) {
  constexpr std::array<int, 8> heights{14, 18, 23, 30, 38, 46, 54, 62};
  return heights[static_cast<std::size_t>(std::clamp(scale, 1, 8) - 1)];
}

const UiGlyphMetric& ui_glyph(char character, bool heading) {
  const unsigned char value = static_cast<unsigned char>(character);
  const int codepoint = value >= kUiFontFirstCodepoint &&
                                value <= kUiFontLastCodepoint
                            ? value
                            : '?';
  const auto index = static_cast<std::size_t>(codepoint - kUiFontFirstCodepoint);
  return heading ? kUiFontHeadingMetrics[index] : kUiFontRegularMetrics[index];
}

int text_width(std::string_view text, int scale) {
  if (text.empty()) {
    return 0;
  }
  const bool heading = scale >= 3;
  const double ratio = static_cast<double>(font_height(scale)) / kUiFontSourceSize;
  int width = 0;
  for (const char character : text) {
    width += static_cast<int>(std::round(ui_glyph(character, heading).advance * ratio));
  }
  return width;
}

void draw_text(SDL_Renderer* renderer, std::string_view text, int x, int y, int scale,
               Color color) {
  const bool heading = scale >= 3;
  SDL_Texture* texture = cached_texture(
      renderer, executable_asset(heading ? "fonts/nunito-extrabold.png"
                                         : "fonts/nunito-semibold.png"));
  if (texture == nullptr) {
    set_color(renderer, color);
    for (const char character : text) {
      const auto rows = glyph(character);
      for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
          if ((rows[row] & (1U << (4 - column))) == 0) continue;
          const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
      x += 6 * scale;
    }
    return;
  }
  SDL_SetTextureColorMod(texture, color.red, color.green, color.blue);
  SDL_SetTextureAlphaMod(texture, color.alpha);
  const int height = font_height(scale);
  const double ratio = static_cast<double>(height) / kUiFontSourceSize;
  const int ascent = heading ? kUiFontHeadingAscent : kUiFontRegularAscent;
  for (const char character : text) {
    const auto& metric = ui_glyph(character, heading);
    if (metric.width > 0 && metric.height > 0) {
      const SDL_Rect source{metric.source_x, metric.source_y, metric.width,
                            metric.height};
      const SDL_Rect destination{
          x + static_cast<int>(std::round(metric.bearing_x * ratio)),
          y + static_cast<int>(std::round((ascent + metric.bearing_top) * ratio)),
          std::max(1, static_cast<int>(std::round(metric.width * ratio))),
          std::max(1, static_cast<int>(std::round(metric.height * ratio)))};
      SDL_RenderCopy(renderer, texture, &source, &destination);
    }
    x += static_cast<int>(std::round(metric.advance * ratio));
  }
}

void draw_centered_text(SDL_Renderer* renderer, std::string_view text, int center_x, int y,
                        int scale, Color color) {
  draw_text(renderer, text, center_x - text_width(text, scale) / 2, y, scale, color);
}

void draw_footer(SDL_Renderer* renderer, std::string_view text) {
  const SDL_Rect footer{64, 438, 512, 31};
  fill_rect(renderer, footer, kPanel);
  draw_centered_text(renderer, text, kWidth / 2, 445, 1, kMuted);
}

void draw_heading_panel(SDL_Renderer* renderer, const SDL_Rect& bounds,
                        std::string_view title, std::string_view subtitle = {}) {
  fill_rect(renderer, bounds, {255, 250, 231, 238});
  draw_centered_text(renderer, title, bounds.x + bounds.w / 2,
                     bounds.y + 10, 3, kText);
  if (!subtitle.empty()) {
    draw_centered_text(renderer, subtitle, bounds.x + bounds.w / 2,
                       bounds.y + 43, 1, kMuted);
  }
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void render_storybook_background(
    SDL_Renderer* renderer,
    const std::filesystem::path& requested = std::filesystem::path{}) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  const auto path = requested.empty()
                        ? executable_asset("backgrounds/garden-morning.png")
                        : requested;
  SDL_Texture* texture = cached_texture(renderer, path);
  if (texture != nullptr) {
    const SDL_Rect canvas{0, 0, kWidth, kHeight};
    SDL_RenderCopy(renderer, texture, nullptr, &canvas);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    set_color(renderer, {255, 250, 229, 34});
    SDL_RenderFillRect(renderer, &canvas);
  }
}

bool render_treated_portrait(SDL_Renderer* renderer,
                             const std::filesystem::path& path,
                             const SDL_Rect& destination, Color border,
                             float border_radius = 5.0F,
                             double angle = 0.0) {
  struct CachedPortrait {
    SDL_Renderer* renderer;
    std::string path;
    Color border;
    int radius_tenths;
    SDL_Texture* border_texture;
    SDL_Texture* source_texture;
  };
  static std::vector<CachedPortrait> cache;
  const std::string encoded_path = path_as_utf8(path);
  const int radius_tenths = static_cast<int>(border_radius * 10.0F);
  const auto cached = std::find_if(
      cache.begin(), cache.end(), [&](const CachedPortrait& candidate) {
        return candidate.renderer == renderer && candidate.path == encoded_path &&
               candidate.border.red == border.red &&
               candidate.border.green == border.green &&
               candidate.border.blue == border.blue &&
               candidate.border.alpha == border.alpha &&
               candidate.radius_tenths == radius_tenths;
      });
  if (cached != cache.end()) {
    SDL_RenderCopyEx(renderer, cached->border_texture, nullptr, &destination,
                     angle, nullptr, SDL_FLIP_NONE);
    SDL_RenderCopyEx(renderer, cached->source_texture, nullptr, &destination,
                     angle, nullptr, SDL_FLIP_NONE);
    return true;
  }

  SDL_Surface* loaded = IMG_Load(encoded_path.c_str());
  if (loaded == nullptr) return false;
  SDL_Surface* source =
      SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded);
  if (source == nullptr) return false;

  std::vector<std::uint8_t> alpha(
      static_cast<std::size_t>(source->w * source->h));
  const auto* pixels = static_cast<const std::uint8_t*>(source->pixels);
  for (int y = 0; y < source->h; ++y) {
    for (int x = 0; x < source->w; ++x) {
      alpha[static_cast<std::size_t>(y * source->w + x)] =
          pixels[y * source->pitch + x * 4 + 3];
    }
  }

  const auto padded =
      pad_portrait_alpha(alpha, source->w, source->h, border_radius);
  SDL_Surface* padded_source = SDL_CreateRGBSurfaceWithFormat(
      0, padded.width, padded.height, 32, SDL_PIXELFORMAT_RGBA32);
  if (padded_source == nullptr) {
    SDL_FreeSurface(source);
    return false;
  }
  SDL_FillRect(padded_source, nullptr,
               SDL_MapRGBA(padded_source->format, 0, 0, 0, 0));
  SDL_Rect source_destination{padded.inset, padded.inset, source->w, source->h};
  SDL_BlitSurface(source, nullptr, padded_source, &source_destination);
  SDL_FreeSurface(source);
  source = padded_source;
  alpha = padded.alpha;

  const auto mask =
      smooth_portrait_outline(alpha, source->w, source->h, border_radius);
  SDL_Surface* border_surface = SDL_CreateRGBSurfaceWithFormat(
      0, source->w, source->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (border_surface == nullptr) {
    SDL_FreeSurface(source);
    return false;
  }
  auto* border_pixels = static_cast<std::uint8_t*>(border_surface->pixels);
  for (int y = 0; y < source->h; ++y) {
    for (int x = 0; x < source->w; ++x) {
      const auto index = static_cast<std::size_t>(y * source->w + x);
      auto* pixel = border_pixels + y * border_surface->pitch + x * 4;
      pixel[0] = border.red;
      pixel[1] = border.green;
      pixel[2] = border.blue;
      pixel[3] = static_cast<std::uint8_t>(
          static_cast<unsigned int>(mask[index]) * border.alpha / 255U);
    }
  }

  const int source_width = source->w;
  const int source_height = source->h;
  SDL_Texture* border_texture =
      SDL_CreateTextureFromSurface(renderer, border_surface);
  SDL_Texture* source_texture = SDL_CreateTextureFromSurface(renderer, source);
  SDL_FreeSurface(border_surface);
  SDL_FreeSurface(source);
  if (border_texture == nullptr || source_texture == nullptr) {
    std::cerr << "SPROUT_TEXTURE portrait-create-failed path=" << encoded_path
              << " dimensions=" << source_width << 'x' << source_height
              << " border=" << (border_texture == nullptr ? "failed" : "ok")
              << " source=" << (source_texture == nullptr ? "failed" : "ok")
              << " error=" << SDL_GetError() << '\n';
    if (border_texture != nullptr) SDL_DestroyTexture(border_texture);
    if (source_texture != nullptr) SDL_DestroyTexture(source_texture);
    return false;
  }
  SDL_SetTextureBlendMode(border_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_BLEND);
  SDL_RenderCopyEx(renderer, border_texture, nullptr, &destination, angle,
                   nullptr, SDL_FLIP_NONE);
  SDL_RenderCopyEx(renderer, source_texture, nullptr, &destination, angle,
                   nullptr, SDL_FLIP_NONE);
  cache.push_back({renderer, encoded_path, border, radius_tenths,
                   border_texture, source_texture});
  return true;
}

void render_profile_select(SDL_Renderer* renderer, const LauncherState& state,
                           const std::filesystem::path& managed_image_root,
                           const std::filesystem::path& built_in_avatar_root) {
  const auto slots = profile_select_layout(
      static_cast<int>(state.profiles().size()),
      static_cast<int>(state.focus_index()));
  const bool static_ui = SDL_getenv("SPROUT_STATIC_UI") != nullptr;
  const double phase = static_ui
                           ? 0.0
                           : static_cast<double>(SDL_GetTicks64() % 2400U) /
                                 2400.0 * 2.0 * 3.141592653589793;

  for (std::size_t index = 0; index < state.profiles().size(); ++index) {
    const auto& profile = state.profiles()[index];
    const bool focused = index == state.focus_index();
    const auto& slot = slots[index];
    const int center_x = slot.center_x;
    const int center_y = slot.center_y;
    const int size = slot.avatar_size;
    const int bob =
        focused ? static_cast<int>(std::round(std::sin(phase) * 4.0)) : 0;
    const double angle = focused ? std::sin(phase) * 2.25 : 0.0;
    const SDL_Rect avatar{center_x - size / 2,
                          center_y - size / 2 + bob, size, size};
    bool rendered_portrait = false;
    if (!managed_image_root.empty() &&
        starts_with(profile.avatar_ref, "local:")) {
      try {
        const auto path = ProfileImageImporter::resolve_portrait_at(
            managed_image_root, profile.avatar_ref);
        rendered_portrait = render_treated_portrait(
            renderer, path, avatar, focused ? kHoney : Color{255, 250, 231},
            focused ? 8.0F : 4.0F, angle);
      } catch (const std::exception&) {
      }
    } else if (!built_in_avatar_root.empty()) {
      const auto* built_in = find_built_in_avatar(profile.avatar_ref);
      if (built_in != nullptr) {
        try {
          const auto path = built_in_avatar_thumbnail_path(
              built_in_avatar_root, built_in->id);
          rendered_portrait = render_treated_portrait(
              renderer, path, avatar, focused ? kHoney : Color{255, 250, 231},
              focused ? 8.0F : 4.0F, angle);
        } catch (const std::exception&) {
        }
      }
    }
    if (!rendered_portrait) {
      const std::string initial(1, profile.display_name.front());
      draw_centered_text(renderer, initial, center_x, center_y - 32 + bob, 8,
                         kText);
    }

    const SDL_Rect name_panel{center_x - (focused ? 70 : 62),
                              center_y + size / 2 + 12,
                              focused ? 140 : 124, focused ? 38 : 34};
    fill_rect(renderer, name_panel, {255, 250, 231, 230});
    draw_centered_text(renderer, profile.display_name, center_x,
                       name_panel.y + (focused ? 7 : 6),
                       focused ? 3 : 2, kText);
  }
}

void render_home(SDL_Renderer* renderer, const LauncherState& state,
                 const std::filesystem::path& managed_image_root,
                 const std::filesystem::path& built_in_avatar_root,
                 const std::filesystem::path& current_background) {
  const auto* profile = state.active_profile();
  if (profile == nullptr) {
    return;
  }

  if (state.screen() == Screen::ChildHome) {
    constexpr SDL_Rect avatar_card{48, 92, 248, 286};
    constexpr SDL_Rect background_card{344, 92, 248, 286};
    constexpr SDL_Rect avatar_rect{72, 112, 200, 240};
    constexpr SDL_Rect background_rect{368, 112, 200, 240};
    fill_rect(renderer, avatar_card, kPanel);
    fill_rect(renderer, background_card, kPanel);
    outline_rect(renderer,
                 state.focus_index() == 0 ? avatar_card : background_card,
                 6, kHoney);

    bool rendered_portrait = false;
    if (!managed_image_root.empty() &&
        starts_with(profile->avatar_ref, "local:")) {
      try {
        rendered_portrait = render_treated_portrait(
            renderer,
            ProfileImageImporter::resolve_portrait_at(managed_image_root,
                                                       profile->avatar_ref),
            avatar_rect, Color{255, 250, 231}, 5.0F);
      } catch (const std::exception&) {
      }
    } else if (!built_in_avatar_root.empty()) {
      if (const auto* built_in = find_built_in_avatar(profile->avatar_ref);
          built_in != nullptr) {
        try {
          rendered_portrait = render_treated_portrait(
              renderer,
              built_in_avatar_thumbnail_path(built_in_avatar_root,
                                             built_in->id),
              avatar_rect, Color{255, 250, 231}, 5.0F);
        } catch (const std::exception&) {
        }
      }
    }
    if (!rendered_portrait) {
      const std::string initial(1, profile->display_name.front());
      draw_centered_text(renderer, initial, avatar_rect.x + avatar_rect.w / 2,
                         avatar_rect.y + 70, 8, kText);
    }

    if (SDL_Texture* background = cached_texture(renderer, current_background);
        background != nullptr) {
      SDL_RenderCopy(renderer, background, nullptr, &background_rect);
    } else {
      fill_rect(renderer, background_rect, {114, 164, 126, 220});
    }
    return;
  }

  fill_rect(renderer, {54, 16, 532, 72}, kPanel);
  draw_centered_text(renderer, "Hello, " + profile->display_name + "!",
                     kWidth / 2, 27, 4, kText);
  draw_centered_text(renderer, "PARENT MENU", kWidth / 2, 68, 1, kMuted);

  const auto items = state.menu_items();
  const int row_start = items.size() > 8 ? 98 : (items.size() > 7 ? 104 : (items.size() > 6 ? 108 : 118));
  const int row_gap = items.size() > 8 ? 34 : (items.size() > 7 ? 38 : (items.size() > 6 ? 43 : 49));
  const int row_height = items.size() > 8 ? 27 : (items.size() > 7 ? 31 : (items.size() > 6 ? 35 : 39));
  for (std::size_t index = 0; index < items.size(); ++index) {
    const bool focused = index == state.focus_index();
    const SDL_Rect row{focused ? 148 : 156,
                       row_start + static_cast<int>(index) * row_gap,
                       focused ? 344 : 328, row_height};
    fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
    if (focused) {
      outline_rect(renderer, row, 3, kHoney);
    }
    draw_text(renderer, items[index], row.x + 24,
              row.y + (row_height - font_height(2)) / 2 - 2, 2, kText);
  }

  fill_rect(renderer, {150, 438, 340, 30}, {255, 250, 231, 210});
  draw_centered_text(renderer, "A CHOOSE   B PROFILES", kWidth / 2, 444, 1,
                     kMuted);
}

int setup_step_number(SetupStep step) {
  return step == SetupStep::Complete ? 11 : static_cast<int>(step) + 1;
}

}  // namespace

bool render_startup_splash(SDL_Renderer* renderer,
                           const std::filesystem::path& image_path) {
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  SDL_Texture* texture = cached_texture(renderer, image_path);
  if (texture == nullptr) {
    // mmiyoo rejects textures larger than its hardware limit. Never leave the
    // framebuffer black just because optional startup artwork could not load.
    fill_rect(renderer, {158, 174, 324, 112}, {255, 250, 229, 238});
    draw_centered_text(renderer, "SproutOS", kWidth / 2, 194, 6, kText);
    draw_centered_text(renderer, "GROWING YOUR LIBRARY", kWidth / 2, 254, 1,
                       kMuted);
    present_frame(renderer);
    return false;
  }
  int image_width = 0;
  int image_height = 0;
  if (SDL_QueryTexture(texture, nullptr, nullptr, &image_width, &image_height) != 0 ||
      image_width <= 0 || image_height <= 0) {
    present_frame(renderer);
    return false;
  }
  const double scale = std::min(static_cast<double>(kWidth) / image_width,
                                static_cast<double>(kHeight) / image_height);
  const int width = std::max(1, static_cast<int>(image_width * scale));
  const int height = std::max(1, static_cast<int>(image_height * scale));
  const SDL_Rect destination{(kWidth - width) / 2, (kHeight - height) / 2,
                             width, height};
  SDL_RenderCopy(renderer, texture, nullptr, &destination);
  present_frame(renderer);
  return true;
}

void render_launcher(SDL_Renderer* renderer, const LauncherState& state,
                     const std::filesystem::path& managed_image_root,
                     const std::filesystem::path& background_image,
                     const std::filesystem::path& accent_atlas,
                     const std::filesystem::path& built_in_avatar_root) {
  std::filesystem::path resolved_background = background_image;
  if (state.screen() != Screen::ProfileSelect && state.active_profile() != nullptr) {
    constexpr std::string_view prefix = "builtin:";
    const auto& reference = state.active_profile()->background_ref;
    if (sprout::launcher::starts_with(reference, prefix)) {
      if (const auto* background =
              find_built_in_background(std::string_view(reference).substr(prefix.size()));
          background != nullptr) {
        resolved_background = executable_asset(
            "backgrounds/" + std::string(background->filename));
      }
    }
  }
  const bool profile_selector = state.screen() == Screen::ProfileSelect;
  if (profile_selector) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
  } else {
    render_storybook_background(renderer, resolved_background);
  }

  if (!profile_selector && !accent_atlas.empty()) {
    SDL_Texture* accents =
        IMG_LoadTexture(renderer, path_as_utf8(accent_atlas).c_str());
    if (accents != nullptr) {
      SDL_SetTextureAlphaMod(accents, 220);
      const std::array<std::pair<SDL_Rect, SDL_Rect>, 6> decorations{{
          {{2, 2, 226, 211}, {18, 16, 44, 41}},
          {{2, 217, 178, 175}, {578, 28, 30, 30}},
          {{729, 217, 212, 194}, {22, 408, 35, 32}},
          {{876, 415, 80, 77}, {596, 390, 18, 17}},
          {{2, 217, 178, 175}, {44, 74, 18, 18}},
          {{2, 2, 226, 211}, {568, 416, 34, 32}},
      }};
      for (const auto& [source, destination] : decorations) {
        SDL_RenderCopy(renderer, accents, &source, &destination);
      }
      SDL_DestroyTexture(accents);
    }
  }

  if (state.screen() == Screen::ProfileSelect) {
    render_profile_select(renderer, state, managed_image_root,
                          built_in_avatar_root);
  } else {
    render_home(renderer, state, managed_image_root, built_in_avatar_root,
                resolved_background);
  }

  present_frame(renderer);
}

void render_library(
    SDL_Renderer* renderer, const LibraryPresentation& library,
    const Profile* active_profile,
    const std::filesystem::path& managed_image_root,
    const std::filesystem::path& built_in_avatar_root) {
  render_storybook_background(renderer);

  fill_rect(renderer, {14, 8, 612, 42}, {255, 250, 231, 238});
  if (active_profile == nullptr) {
    draw_text(renderer, "GAME LIBRARY", 28, 18, 3, kText);
  } else {
    const SDL_Rect portrait{24, 12, 34, 34};
    bool rendered_portrait = false;
    if (!managed_image_root.empty() &&
        starts_with(active_profile->avatar_ref, "local:")) {
      try {
        rendered_portrait = render_treated_portrait(
            renderer,
            ProfileImageImporter::resolve_portrait_at(
                managed_image_root, active_profile->avatar_ref),
            portrait, kHoney, 4.0F);
      } catch (const std::exception&) {
      }
    } else if (!built_in_avatar_root.empty()) {
      if (const auto* avatar = find_built_in_avatar(active_profile->avatar_ref);
          avatar != nullptr) {
        try {
          rendered_portrait = render_treated_portrait(
              renderer,
              built_in_avatar_thumbnail_path(built_in_avatar_root, avatar->id),
              portrait, kHoney, 4.0F);
        } catch (const std::exception&) {
        }
      }
    }
    if (!rendered_portrait) {
      fill_rect(renderer, portrait, kPanelFocused);
      draw_centered_text(
          renderer,
          std::string(1, active_profile->display_name.empty()
                             ? '?'
                             : active_profile->display_name.front()),
          portrait.x + portrait.w / 2, portrait.y + 7, 3, kText);
    }
    draw_text(renderer, active_profile->display_name, 68, 18, 3, kText);
  }
  constexpr std::array<LibrarySection, 4> sections{
      LibrarySection::Recent, LibrarySection::Favorites,
      LibrarySection::All, LibrarySection::Arcade};
  constexpr std::array<std::string_view, 4> section_labels{
      "CONTINUE", "FAVORITES", "ALL GAMES", "SPROUT ARCADE"};
  const auto section_iterator =
      std::find(sections.begin(), sections.end(), library.section());
  const std::size_t active_section = static_cast<std::size_t>(
      std::distance(sections.begin(), section_iterator));
  const std::size_t previous_section =
      (active_section + sections.size() - 1U) % sections.size();
  const std::size_t next_section = (active_section + 1U) % sections.size();

  // Neighboring rail headers intentionally peek into the viewport. Their
  // position, not instructional copy, makes vertical carousel navigation
  // discoverable on the 640x480 display.
  fill_rect(renderer, {18, 54, 604, 28}, {255, 250, 231, 190});
  draw_centered_text(renderer, section_labels[previous_section], kWidth / 2,
                     62, 1, kMuted);
  fill_rect(renderer, {76, 84, 488, 28}, {255, 250, 231, 238});
  draw_centered_text(renderer, section_labels[active_section], kWidth / 2,
                     90, 2, kText);

  const auto active_entries = library.entries(library.section());
  if (active_entries.empty()) {
    const SDL_Rect empty{118, 154, 404, 112};
    fill_rect(renderer, empty, {255, 250, 231, 232});
    draw_centered_text(renderer, "Nothing here yet", kWidth / 2, 192, 3,
                       kMuted);
  } else {
    const std::size_t focus = library.focus_index(library.section());
    const std::size_t count = active_entries.size();
    constexpr std::array<int, 3> offsets{-1, 1, 0};
    for (const int offset : offsets) {
      const auto signed_index = static_cast<long long>(focus) + offset;
      const std::size_t index = static_cast<std::size_t>(
          (signed_index + static_cast<long long>(count)) %
          static_cast<long long>(count));
      const bool focused = offset == 0;
      const SDL_Rect card = focused
                                ? SDL_Rect{120, 118, 400, 215}
                                : SDL_Rect{offset < 0 ? -90 : 470, 156, 260, 140};
      fill_rect(renderer, {card.x + 7, card.y + 9, card.w, card.h},
                {23, 49, 39, 150});
      fill_rect(renderer, card, {255, 250, 231, 246});

      const auto& entry = active_entries[index];
      std::filesystem::path artwork = entry.artwork_path;
      if (!artwork.empty() && artwork.is_relative()) {
        artwork = executable_asset(artwork.generic_string());
      }
      SDL_Texture* texture = artwork.empty() ? nullptr
                                             : cached_texture(renderer, artwork);
      const SDL_Rect image_area{card.x + 5, card.y + 5, card.w - 10,
                                card.h - 10};
      if (texture != nullptr) {
        int texture_width = 0;
        int texture_height = 0;
        if (SDL_QueryTexture(texture, nullptr, nullptr, &texture_width,
                             &texture_height) == 0 &&
            texture_width > 0 && texture_height > 0) {
          const double scale = std::min(
              static_cast<double>(image_area.w) / texture_width,
              static_cast<double>(image_area.h) / texture_height);
          const int width = std::max(1, static_cast<int>(texture_width * scale));
          const int height =
              std::max(1, static_cast<int>(texture_height * scale));
          const SDL_Rect destination{
              image_area.x + (image_area.w - width) / 2,
              image_area.y + (image_area.h - height) / 2, width, height};
          SDL_RenderCopy(renderer, texture, nullptr, &destination);
        }
      } else {
        const std::uint32_t seed = static_cast<std::uint32_t>(
            std::hash<std::string>{}(entry.id));
        fill_rect(renderer, image_area,
                  {static_cast<std::uint8_t>(70 + seed % 90),
                   static_cast<std::uint8_t>(80 + (seed >> 8U) % 100),
                   static_cast<std::uint8_t>(100 + (seed >> 16U) % 110)});
      }
      outline_rect(renderer, card, focused ? 5 : 2,
                   focused ? kFocus : Color{255, 250, 231, 210});
    }
  }

  fill_rect(renderer, {18, 356, 604, 106}, {255, 250, 231, 205});
  draw_centered_text(renderer, section_labels[next_section], kWidth / 2, 370,
                     2, kText);
  outline_rect(renderer, {18, 356, 604, 106}, 2,
               Color{255, 250, 231, 220});

  if (!library.notice().empty()) {
    fill_rect(renderer, {118, 418, 404, 34}, {255, 226, 155, 246});
    draw_centered_text(renderer, library.notice().substr(0, 68), kWidth / 2,
                       427, 1, kFocus);
  }
  present_frame(renderer);
}

void render_profile_archive(SDL_Renderer* renderer,
                            const ProfileArchivePresentation& archive) {
  render_storybook_background(renderer);

  draw_heading_panel(renderer, {82, 16, 476, 78}, archive.title(),
                     archive.description());

  const auto choices = archive.choices();
  constexpr std::size_t visible_count = 6;
  const std::size_t first = archive.focus_index() < visible_count
                                ? 0
                                : archive.focus_index() - visible_count + 1;
  const std::size_t last = std::min(choices.size(), first + visible_count);
  for (std::size_t index = first; index < last; ++index) {
    const int row_number = static_cast<int>(index - first);
    const SDL_Rect row{78, 120 + row_number * 48, 484, 38};
    const bool focused = index == archive.focus_index();
    fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
    if (focused) {
      outline_rect(renderer, row, 3, kFocus);
      draw_text(renderer, ">", 94, row.y + 12, 2, kFocus);
    }
    draw_text(renderer, choices[index].substr(0, 34), 124, row.y + 12, 2,
              kText);
  }

  if (!archive.notice().empty()) {
    draw_centered_text(renderer,
                       std::string(archive.notice()).substr(0, 68), kWidth / 2,
                       414, 1,
                       archive.notice_is_error() ? kFocus : kText);
  }
  draw_footer(renderer, "ARROWS MOVE   A SELECT   B BACK");
  present_frame(renderer);
}

void render_profile_avatars(
    SDL_Renderer* renderer, const ProfileAvatarPresentation& presentation,
    const std::filesystem::path& built_in_avatar_root) {
  render_storybook_background(renderer);

  fill_rect(renderer, {82, 14, 476, 72}, {255, 250, 231, 238});
  draw_centered_text(renderer, "PROFILE APPEARANCE", kWidth / 2, 24, 4, kText);
  if (presentation.stage() == ProfileAvatarStage::Profile) {
    draw_centered_text(renderer, "CHOOSE A PROFILE TO CUSTOMIZE", kWidth / 2,
                       70, 1, kMuted);
    const auto profiles = presentation.profiles();
    for (std::size_t index = 0; index < profiles.size(); ++index) {
      const SDL_Rect row{92, 116 + static_cast<int>(index) * 62, 456, 48};
      fill_rect(renderer, row,
                index == presentation.focus_index() ? kPanelFocused : kPanel);
      if (index == presentation.focus_index()) {
        outline_rect(renderer, row, 3, kFocus);
      }
      draw_text(renderer, profiles[index].display_name, 118, row.y + 11, 3,
                kText);
      draw_text(renderer,
                profiles[index].role == ProfileRole::Child ? "CHILD" : "PARENT",
                438, row.y + 17, 1, kMuted);
    }
  } else if (presentation.stage() == ProfileAvatarStage::Appearance) {
    const auto* profile = presentation.selected_profile();
    draw_centered_text(renderer,
                       profile == nullptr ? "WHAT WOULD YOU LIKE TO CHANGE?"
                                          : "CUSTOMIZE " + profile->display_name,
                       kWidth / 2, 74, 2, kMuted);
    constexpr std::array<std::string_view, 2> choices{
        "PROFILE IMAGE", "HOME BACKGROUND"};
    for (std::size_t index = 0; index < choices.size(); ++index) {
      const SDL_Rect card{90 + static_cast<int>(index) * 250, 142, 210, 190};
      fill_rect(renderer, card,
                index == presentation.focus_index() ? kPanelFocused : kPanel);
      if (index == presentation.focus_index()) outline_rect(renderer, card, 4, kFocus);
      draw_centered_text(renderer, index == 0 ? "FACE" : "SCENE",
                         card.x + card.w / 2, card.y + 46, 5, kFocus);
      draw_centered_text(renderer, choices[index], card.x + card.w / 2,
                         card.y + 132, 2, kText);
    }
  } else if (presentation.stage() == ProfileAvatarStage::Background) {
    const auto* profile = presentation.selected_profile();
    draw_centered_text(renderer,
                       profile == nullptr ? "CHOOSE A HOME BACKGROUND"
                                          : "BACKGROUND FOR " + profile->display_name,
                       kWidth / 2, 68, 2, kMuted);
    const auto backgrounds = presentation.backgrounds();
    for (std::size_t index = 0; index < backgrounds.size(); ++index) {
      const int column = static_cast<int>(index % 2U);
      const int row = static_cast<int>(index / 2U);
      const SDL_Rect cell{72 + column * 258, 108 + row * 142, 238, 124};
      fill_rect(renderer, cell,
                index == presentation.focus_index() ? kPanelFocused : kPanel);
      SDL_Texture* texture = cached_texture(
          renderer, executable_asset("backgrounds/" +
                                     std::string(backgrounds[index].filename)));
      if (texture != nullptr) {
        const SDL_Rect image{cell.x + 6, cell.y + 6, cell.w - 12, 88};
        SDL_RenderCopy(renderer, texture, nullptr, &image);
      }
      if (index == presentation.focus_index()) outline_rect(renderer, cell, 4, kFocus);
      draw_centered_text(renderer, backgrounds[index].display_name,
                         cell.x + cell.w / 2, cell.y + 98, 1, kText);
    }
    const SDL_Rect header{82, 16, 476, 76};
    fill_rect(renderer, header, kPanel);
    draw_centered_text(renderer, "PROFILE APPEARANCE", kWidth / 2, 24, 4, kText);
    draw_centered_text(renderer,
                       profile == nullptr ? "CHOOSE A HOME BACKGROUND"
                                          : "BACKGROUND FOR " + profile->display_name,
                       kWidth / 2, 68, 2, kMuted);
  } else {
    const auto* profile = presentation.selected_profile();
    draw_centered_text(
        renderer,
        profile == nullptr ? "CHOOSE A PORTRAIT"
                           : "PORTRAIT FOR " + profile->display_name,
        kWidth / 2, 68, 2, kMuted);

    constexpr int cell_size = 112;
    constexpr int gap_x = 20;
    constexpr int gap_y = 20;
    constexpr int start_x = 66;
    constexpr int start_y = 102;
    const auto avatars = presentation.avatars();
    const std::size_t first = presentation.page_index() * 8U;
    const std::size_t total = avatars.size() +
                              (presentation.custom_image_available() ? 1U : 0U);
    const std::size_t last = std::min(total, first + 8U);
    for (std::size_t index = first; index < last; ++index) {
      const std::size_t local_index = index - first;
      const int column = static_cast<int>(local_index % 4U);
      const int row = static_cast<int>(local_index / 4U);
      const SDL_Rect cell{start_x + column * (cell_size + gap_x),
                          start_y + row * (cell_size + gap_y), cell_size,
                          cell_size};
      fill_rect(renderer, cell,
                index == presentation.focus_index() ? kPanelFocused : kPanel);
      if (index == presentation.focus_index()) {
        outline_rect(renderer, cell, 4, kFocus);
      }
      if (index < avatars.size()) {
        try {
          const auto path = built_in_avatar_thumbnail_path(
              built_in_avatar_root, avatars[index].id);
          const SDL_Rect image{cell.x + 8, cell.y + 8, 96, 96};
          static_cast<void>(render_treated_portrait(
              renderer, path, image,
              index == presentation.focus_index() ? kFocus : kMuted));
        } catch (const std::exception&) {
        }
      } else {
        draw_centered_text(renderer, "+", cell.x + cell.w / 2, cell.y + 24, 8,
                           kFocus);
        draw_centered_text(renderer, "CUSTOM", cell.x + cell.w / 2,
                           cell.y + 82, 1, kText);
      }
    }

    std::string label = "IMPORT CUSTOM IMAGE";
    if (!presentation.import_focused() &&
        presentation.focus_index() < avatars.size()) {
      label = std::string(avatars[presentation.focus_index()].label);
    }
    fill_rect(renderer, {150, 366, 340, 66}, {255, 250, 231, 232});
    draw_centered_text(renderer, label, kWidth / 2, 380, 2, kText);
    draw_centered_text(
        renderer,
        "PAGE " + std::to_string(presentation.page_index() + 1U) + " OF " +
            std::to_string(presentation.page_count()),
        kWidth / 2, 410, 1, kMuted);
  }

  if (!presentation.notice().empty()) {
    draw_centered_text(renderer, presentation.notice(), kWidth / 2, 430, 1,
                       kFocus);
  }
  draw_footer(renderer, "ARROWS MOVE   A SELECT   B BACK");
  present_frame(renderer);
}

void render_profile_image_crop(SDL_Renderer* renderer,
                               const ProfileImageCropPresentation& crop) {
  render_storybook_background(renderer);
  draw_heading_panel(renderer, {82, 16, 476, 78}, "CROP PROFILE PORTRAIT",
                     "MOVE THE PHOTO INSIDE THE SQUARE");

  const auto pixels = crop.preview_rgba();
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, 256, 256);
  if (texture != nullptr) {
    SDL_UpdateTexture(texture, nullptr, pixels.data(), 256 * 4);
    const SDL_Rect destination{192, 110, 256, 256};
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
    SDL_DestroyTexture(texture);
    outline_rect(renderer, destination, 4, kFocus);
  }

  const int zoom_percent = static_cast<int>(crop.selection().zoom * 100.0);
  fill_rect(renderer, {222, 376, 196, 36}, {255, 250, 231, 232});
  draw_centered_text(renderer, "ZOOM " + std::to_string(zoom_percent) + "%",
                     kWidth / 2, 386, 2, kText);
  if (!crop.error_message().empty()) {
    draw_centered_text(renderer, crop.error_message().substr(0, 68), kWidth / 2, 414, 1,
                       kFocus);
  }
  draw_footer(renderer, "ARROWS MOVE   L R ZOOM   A USE   B CANCEL");
  present_frame(renderer);
}

void render_parent_pin(SDL_Renderer* renderer,
                       const ParentPinPresentation& pin) {
  render_storybook_background(renderer);
  draw_heading_panel(renderer, {82, 12, 476, 70}, pin.title(),
                     pin.description());

  const SDL_Rect pin_field{196, 88, 248, 48};
  fill_rect(renderer, pin_field, kPanel);
  outline_rect(renderer, pin_field, 2, kPanelFocused);
  const std::string masked(pin.entered_digits(), '*');
  draw_centered_text(renderer, masked.empty() ? "-" : masked, kWidth / 2, 101, 3,
                     kText);

  const auto choices = pin.choices();
  for (std::size_t index = 0; index < choices.size(); ++index) {
    const int column = static_cast<int>(index % 3);
    const int row = static_cast<int>(index / 3);
    const SDL_Rect key{170 + column * 105, 158 + row * 58, 90, 44};
    fill_rect(renderer, key,
              index == pin.focus_index() ? kPanelFocused : kPanel);
    if (index == pin.focus_index()) {
      outline_rect(renderer, key, 3, kFocus);
    }
    draw_centered_text(renderer, choices[index], key.x + key.w / 2, key.y + 13,
                       choices[index].size() > 2 ? 1 : 2, kText);
  }

  if (!pin.error_message().empty()) {
    draw_centered_text(renderer, pin.error_message().substr(0, 60), kWidth / 2, 406, 1,
                       kFocus);
  }
  draw_footer(renderer, "ARROWS MOVE   A SELECT   B CANCEL");
  present_frame(renderer);
}

void render_setup(SDL_Renderer* renderer, const SetupPresentation& setup) {
  render_storybook_background(renderer);

  const std::string progress = "STEP " + std::to_string(setup_step_number(setup.step())) +
                               " OF 11";
  fill_rect(renderer, {18, 12, 604, 44}, {255, 250, 231, 238});
  draw_text(renderer, "SPROUT", 34, 21, 3, kText);
  draw_text(renderer, progress, 510, 26, 1, kMuted);

  const SDL_Rect panel{54, 68, 532, 322};
  fill_rect(renderer, panel, kPanel);
  outline_rect(renderer, panel, 2, kPanelFocused);
  draw_centered_text(renderer, setup.title(), kWidth / 2, 94, 3, kText);
  draw_centered_text(renderer, setup.description(), kWidth / 2, 138, 1, kMuted);

  const auto choices = setup.choices();
  for (std::size_t index = 0; index < choices.size(); ++index) {
    const SDL_Rect choice{124, 202 + static_cast<int>(index) * 58, 392, 44};
    fill_rect(renderer, choice,
              index == setup.focus_index() ? kPanelFocused : kBackground);
    if (index == setup.focus_index()) {
      outline_rect(renderer, choice, 3, kFocus);
      draw_text(renderer, ">", 142, choice.y + 13, 2, kFocus);
    }
    draw_text(renderer, choices[index], 174, choice.y + 13, 2, kText);
  }

  if (!setup.error_message().empty()) {
    const std::string error = setup.error_message().substr(0, 68);
    draw_centered_text(renderer, error, kWidth / 2, 402, 1, kFocus);
  }
  draw_footer(renderer, "ARROWS CHOOSE   A CONTINUE   B SAVE AND EXIT");
  present_frame(renderer);
}

void render_recovery(SDL_Renderer* renderer,
                     const RecoveryPresentation& recovery) {
  render_storybook_background(renderer);

  draw_heading_panel(renderer, {82, 16, 476, 78}, recovery.title(),
                     recovery.description());

  const auto choices = recovery.choices();
  for (std::size_t index = 0; index < choices.size(); ++index) {
    const SDL_Rect row{92, 126 + static_cast<int>(index) * 68, 456, 52};
    fill_rect(renderer, row,
              index == recovery.focus_index() ? kPanelFocused : kPanel);
    if (index == recovery.focus_index()) {
      outline_rect(renderer, row, 3, kFocus);
      draw_text(renderer, ">", 112, row.y + 14, 2, kFocus);
    }
    draw_text(renderer, choices[index], 148, row.y + 14, 2, kText);
  }

  if (!recovery.notice().empty()) {
    draw_centered_text(renderer,
                       std::string(recovery.notice()).substr(0, 68), kWidth / 2,
                       398, 1,
                       recovery.notice_is_error() ? kFocus : kText);
  }
  draw_footer(renderer, "ARROWS MOVE   A SELECT   B BACK");
  present_frame(renderer);
}

}  // namespace sprout::launcher
