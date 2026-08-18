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

Color kBackground{224, 239, 215};
Color kPanel{255, 250, 231, 224};
Color kPanelFocused{255, 244, 196, 244};
Color kText{37, 67, 53};
Color kMuted{83, 108, 91};
Color kFocus{229, 117, 87};
Color kHoney{241, 188, 73};
bool gRoundedTiles{true};

void apply_interface_theme(std::string_view theme) {
  if (theme == "white") {
    kBackground = {244, 246, 248}; kPanel = {255, 255, 255, 230};
    kPanelFocused = {226, 238, 250, 246}; kText = {24, 34, 45};
    kMuted = {80, 96, 112}; kFocus = {48, 116, 194}; kHoney = {222, 163, 51};
  } else if (theme == "black") {
    kBackground = {18, 21, 25}; kPanel = {39, 44, 51, 235};
    kPanelFocused = {69, 78, 90, 248}; kText = {246, 247, 249};
    kMuted = {183, 191, 202}; kFocus = {112, 190, 255}; kHoney = {244, 195, 88};
  } else if (theme == "grey") {
    kBackground = {164, 169, 175}; kPanel = {231, 233, 236, 232};
    kPanelFocused = {255, 255, 255, 248}; kText = {38, 43, 49};
    kMuted = {91, 98, 106}; kFocus = {75, 119, 169}; kHoney = {202, 151, 61};
  } else {
    kBackground = {224, 239, 215}; kPanel = {255, 250, 231, 224};
    kPanelFocused = {255, 244, 196, 244}; kText = {37, 67, 53};
    kMuted = {83, 108, 91}; kFocus = {229, 117, 87}; kHoney = {241, 188, 73};
  }
}

void apply_accent(std::uint32_t rgb) {
  kFocus = {static_cast<std::uint8_t>((rgb >> 16) & 0xffU),
            static_cast<std::uint8_t>((rgb >> 8) & 0xffU),
            static_cast<std::uint8_t>(rgb & 0xffU)};
}

void set_color(SDL_Renderer* renderer, Color color) {
  SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

void fill_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  set_color(renderer, color);
  const int radius = gRoundedTiles ? std::min({14, rect.w / 3, rect.h / 3}) : 0;
  if (radius < 3 || rect.w >= kWidth || rect.h >= kHeight) {
    SDL_RenderFillRect(renderer, &rect);
    return;
  }
  // Draw each pixel row exactly once. The old horizontal/vertical rectangles
  // overlapped in the middle, double-compositing translucent panels into a
  // visibly different tone at their edges.
  for (int row = 0; row < rect.h; ++row) {
    const int edge = std::min(row, rect.h - row - 1);
    int inset = 0;
    if (edge < radius) {
      const double vertical = radius - edge - 0.5;
      inset = static_cast<int>(std::ceil(
          radius - std::sqrt(std::max(0.0, radius * radius - vertical * vertical))));
    }
    SDL_RenderDrawLine(renderer, rect.x + inset, rect.y + row,
                       rect.x + rect.w - inset - 1, rect.y + row);
  }
}

void outline_rect(SDL_Renderer* renderer, SDL_Rect rect, int thickness, Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  set_color(renderer, color);
  for (int index = 0; index < thickness; ++index) {
    const int radius = gRoundedTiles ? std::min({14, rect.w / 3, rect.h / 3}) : 0;
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
  border_radius = gRoundedTiles ? border_radius : 0.0F;
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
      fill_rect(renderer, avatar,
                {static_cast<std::uint8_t>((profile.accent_rgb >> 16) & 0xffU),
                 static_cast<std::uint8_t>((profile.accent_rgb >> 8) & 0xffU),
                 static_cast<std::uint8_t>(profile.accent_rgb & 0xffU), 255});
      if (focused) outline_rect(renderer, avatar, 6, kHoney);
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
  (void)managed_image_root;
  (void)built_in_avatar_root;
  (void)current_background;
  const auto* profile = state.active_profile();
  if (profile == nullptr) {
    return;
  }

  const auto items = state.menu_items();
  const bool child = state.screen() == Screen::ChildHome;
  constexpr SDL_Rect title{210, 24, 220, 52};
  fill_rect(renderer, title, {255, 250, 231, 235});
  draw_centered_text(renderer, child ? "KIDS" : "PARENT", kWidth / 2,
                     title.y + 12, 3, kText);

  const int row_height = 54;
  const int row_gap = 64;
  const int content_height = static_cast<int>(items.size()) * row_gap - 10;
  const int row_start = 96 + (348 - content_height) / 2;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const bool focused = index == state.focus_index();
    const SDL_Rect row{focused ? 112 : 124,
                       row_start + static_cast<int>(index) * row_gap,
                       focused ? 416 : 392, row_height};
    fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
    if (focused) {
      outline_rect(renderer, row, 4, kHoney);
    }
    draw_centered_text(renderer, items[index], kWidth / 2,
                       row.y + (row_height - font_height(2)) / 2 - 2, 2,
                       kText);
  }
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
  std::optional<Color> flat_background;
  if (state.screen() != Screen::ProfileSelect && state.active_profile() != nullptr) {
    constexpr std::string_view prefix = "builtin:";
    const auto& reference = state.active_profile()->background_ref;
    if (sprout::launcher::starts_with(reference, prefix)) {
      if (const auto* background =
              find_built_in_background(std::string_view(reference).substr(prefix.size()));
          background != nullptr) {
        if (background->flat_color) {
          flat_background = Color{
              static_cast<std::uint8_t>((background->color_rgb >> 16) & 0xffU),
              static_cast<std::uint8_t>((background->color_rgb >> 8) & 0xffU),
              static_cast<std::uint8_t>(background->color_rgb & 0xffU), 255};
        } else {
          resolved_background = executable_asset(
              "backgrounds/" + std::string(background->filename));
        }
      }
    }
  }
  const bool profile_selector = state.screen() == Screen::ProfileSelect;
  if (profile_selector) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
  } else {
    if (flat_background.has_value()) {
      set_color(renderer, *flat_background);
      SDL_RenderClear(renderer);
    } else {
      render_storybook_background(renderer, resolved_background);
    }
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

#if 0  // Retired legacy library presentation. The unified dashboard owns game browsing.
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

namespace {

#endif

namespace {

std::string_view dashboard_row_label(DashboardRowKind kind) {
  switch (kind) {
    case DashboardRowKind::NextUp: return "NEXT UP";
    case DashboardRowKind::Recent: return "RECENT";
    case DashboardRowKind::Progress: return "STATS";
    case DashboardRowKind::Platforms: return "PLATFORMS";
    case DashboardRowKind::NeedsReview: return "REVIEW";
    case DashboardRowKind::Unplayed: return "UNOPENED";
    case DashboardRowKind::FinishNext: return "FINISH NEXT";
    case DashboardRowKind::Recommended: return "RECOMMENDED";
    case DashboardRowKind::AllGames: return "ALL GAMES";
    case DashboardRowKind::Hidden: return "HIDDEN";
    case DashboardRowKind::Settings: return "SETTINGS";
  }
  return "GAMES";
}

std::string_view platform_icon_asset(GamePlatform platform) {
  switch (platform) {
    case GamePlatform::GameBoy: return "gb.png";
    case GamePlatform::GameBoyColor: return "gbc.png";
    case GamePlatform::GameBoyAdvance: return "gba.png";
    case GamePlatform::NintendoEntertainmentSystem: return "nes.png";
    case GamePlatform::SuperNintendo: return "snes.png";
    case GamePlatform::SegaGenesis: return "genesis.png";
    case GamePlatform::SegaMasterSystem: return "mastersystem.png";
    case GamePlatform::SegaGameGear: return "gamegear.png";
    case GamePlatform::SegaCD: return "segacd.png";
    case GamePlatform::TurboGrafx16: return "turbografx16.png";
    case GamePlatform::NeoGeo: return "neogeo.png";
    case GamePlatform::OnionArcade: return "arcade.png";
    case GamePlatform::PlayStation: return "playstation.png";
    // These native/catalogue sources do not have a dedicated RetroArch logo,
    // but should still use the proper large arcade glyph rather than the old
    // 16px debug-line fallback.
    case GamePlatform::Pico8:
    case GamePlatform::SproutArcade:
    case GamePlatform::Unknown: return "arcade.png";
  }
  return "arcade.png";
}

void draw_platform_icon(SDL_Renderer* renderer, GamePlatform platform, int x,
                        int y, Color color, int size = 20) {
  if (const auto asset = platform_icon_asset(platform); !asset.empty()) {
    if (SDL_Texture* texture = cached_texture(
            renderer, executable_asset("platform-icons/" + std::string(asset)));
        texture != nullptr) {
      SDL_SetTextureColorMod(texture, color.red, color.green, color.blue);
      SDL_SetTextureAlphaMod(texture, color.alpha);
      const SDL_Rect icon{x, y, size, size};
      SDL_RenderCopy(renderer, texture, nullptr, &icon);
      return;
    }
  }
  set_color(renderer, color);
  const bool handheld = platform == GamePlatform::GameBoy ||
                        platform == GamePlatform::GameBoyColor ||
                        platform == GamePlatform::GameBoyAdvance ||
                        platform == GamePlatform::SegaGameGear;
  if (platform == GamePlatform::SproutArcade) {
    SDL_RenderDrawLine(renderer, x + 7, y + 15, x + 7, y + 7);
    SDL_RenderDrawLine(renderer, x + 7, y + 10, x + 2, y + 5);
    SDL_RenderDrawLine(renderer, x + 7, y + 10, x + 13, y + 4);
    SDL_RenderDrawLine(renderer, x + 2, y + 5, x + 6, y + 5);
    SDL_RenderDrawLine(renderer, x + 13, y + 4, x + 9, y + 5);
    return;
  }
  if (platform == GamePlatform::Pico8) {
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 3; ++column) {
        SDL_Rect pixel{x + column * 5, y + row * 5, 3, 3};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    return;
  }
  if (platform == GamePlatform::OnionArcade) {
    SDL_Rect cabinet{x + 2, y + 1, 12, 15};
    SDL_RenderDrawRect(renderer, &cabinet);
    SDL_Rect screen{x + 4, y + 3, 8, 5};
    SDL_RenderDrawRect(renderer, &screen);
    SDL_RenderDrawPoint(renderer, x + 6, y + 12);
    SDL_RenderDrawPoint(renderer, x + 10, y + 12);
    return;
  }
  if (handheld) {
    SDL_Rect body{x + 2, y, 12, 16};
    SDL_RenderDrawRect(renderer, &body);
    SDL_Rect screen{x + 4, y + 2, 8, 6};
    SDL_RenderDrawRect(renderer, &screen);
    SDL_RenderDrawLine(renderer, x + 5, y + 12, x + 9, y + 12);
    SDL_RenderDrawLine(renderer, x + 7, y + 10, x + 7, y + 14);
    SDL_RenderDrawPoint(renderer, x + 11, y + 11);
    return;
  }
  SDL_Rect console{x, y + 4, 16, 9};
  SDL_RenderDrawRect(renderer, &console);
  SDL_RenderDrawLine(renderer, x + 3, y + 7, x + 8, y + 7);
  SDL_RenderDrawPoint(renderer, x + 12, y + 9);
}

void draw_scroll_right_hint(SDL_Renderer* renderer) {
  if (SDL_Texture* icon = cached_texture(
          renderer, executable_asset("icons/arrow-right.png")); icon != nullptr) {
    SDL_SetTextureColorMod(icon, kText.red, kText.green, kText.blue);
    SDL_SetTextureAlphaMod(icon, 180);
    const SDL_Rect destination{kWidth - 54, kHeight - 54, 32, 32};
    SDL_RenderCopy(renderer, icon, nullptr, &destination);
  }
}

void draw_review_icon(SDL_Renderer* renderer,
                      std::optional<GameReviewVerdict> verdict, int x, int y) {
  const Color color = verdict == GameReviewVerdict::Positive
                          ? Color{52, 132, 83}
                          : verdict == GameReviewVerdict::Negative
                                ? kFocus
                                : kMuted;
  set_color(renderer, color);
  if (!verdict.has_value()) {
    SDL_Rect ring{x + 2, y + 2, 12, 12};
    SDL_RenderDrawRect(renderer, &ring);
    draw_centered_text(renderer, "?", x + 8, y + 5, 1, color);
    return;
  }
  const int direction = *verdict == GameReviewVerdict::Positive ? -1 : 1;
  SDL_Rect palm{x + 5, y + 5, 9, 7};
  SDL_RenderDrawRect(renderer, &palm);
  SDL_RenderDrawLine(renderer, x + 5, y + 8, x + 1,
                     y + 8 + direction * 5);
  SDL_RenderDrawLine(renderer, x + 1, y + 8 + direction * 5, x + 4,
                     y + 8 + direction * 5);
}

void draw_trophy_icon(SDL_Renderer* renderer, int x, int y, bool completed) {
  const Color color = completed ? kHoney : Color{155, 166, 155};
  set_color(renderer, color);
  SDL_Rect cup{x + 4, y + 2, 8, 8};
  SDL_RenderDrawRect(renderer, &cup);
  SDL_RenderDrawLine(renderer, x + 2, y + 4, x + 4, y + 8);
  SDL_RenderDrawLine(renderer, x + 14, y + 4, x + 12, y + 8);
  SDL_RenderDrawLine(renderer, x + 8, y + 10, x + 8, y + 13);
  SDL_RenderDrawLine(renderer, x + 4, y + 14, x + 12, y + 14);
}

std::string compact_time(std::uint64_t milliseconds) {
  const auto minutes = milliseconds / 60000U;
  if (minutes < 60U) return std::to_string(minutes) + "M";
  return std::to_string(minutes / 60U) + "H";
}

std::string review_percent(std::uint64_t reviewed, std::uint64_t total) {
  if (total == 0) return "-";
  return std::to_string((reviewed * 100U) / total) + "%";
}

void render_rounded_artwork(SDL_Renderer* renderer, SDL_Texture* texture,
                            const SDL_Rect& frame, int radius) {
  int texture_width = 0;
  int texture_height = 0;
  if (SDL_QueryTexture(texture, nullptr, nullptr, &texture_width,
                       &texture_height) != 0 ||
      texture_width <= 0 || texture_height <= 0) {
    return;
  }

  // Cards already derive their width from the source aspect ratio.  Rendering
  // to the exact integer bounds avoids a one-pixel letterbox seam without
  // cropping any artwork.
  const SDL_Rect destination = frame;
  for (int line = 0; line < destination.h; ++line) {
    int inset = 0;
    const int edge = std::min(line, destination.h - line - 1);
    if (edge < radius) {
      const double vertical = radius - edge - 0.5;
      inset = static_cast<int>(std::ceil(
          radius - std::sqrt(std::max(0.0, radius * radius -
                                               vertical * vertical))));
    }
    const SDL_Rect source{
        inset * texture_width / destination.w,
        line * texture_height / destination.h,
        std::max(1, (destination.w - inset * 2) * texture_width / destination.w), 1};
    const SDL_Rect target{destination.x + inset, destination.y + line,
                          destination.w - inset * 2, 1};
    SDL_RenderCopy(renderer, texture, &source, &target);
  }
}

int dashboard_card_width(SDL_Renderer* renderer,
                         const GameDashboardPresentation& dashboard,
                         const DashboardGameCard& card, int height) {
  const auto* game = dashboard.find_game(card.item_id);
  if (game == nullptr) return 194;
  auto artwork = game->inventory.artwork_path;
  if (!artwork.empty() && artwork.is_relative()) {
    artwork = executable_asset(artwork.generic_string());
  }
  auto* texture = artwork.empty() ? nullptr : cached_texture(renderer, artwork);
  int width = 0;
  int artwork_height = 0;
  if (texture == nullptr ||
      SDL_QueryTexture(texture, nullptr, nullptr, &width, &artwork_height) != 0 ||
      width <= 0 || artwork_height <= 0) return 194;
  return std::max(80, static_cast<int>(width *
                                       (static_cast<double>(height) / artwork_height)));
}

Color dashboard_row_color(DashboardRowKind kind) {
  constexpr std::array<Color, 11> colors{{
      {246, 194, 101, 238}, {131, 194, 174, 238}, {137, 184, 219, 238},
      {180, 161, 216, 238}, {239, 160, 132, 238}, {128, 188, 201, 238},
      {216, 174, 111, 238}, {142, 196, 139, 238}, {216, 151, 172, 238},
      {154, 163, 170, 238},
      {178, 149, 216, 238},
  }};
  return colors[static_cast<std::size_t>(kind) % colors.size()];
}

void draw_dashboard_row_chip(SDL_Renderer* renderer, DashboardRowKind kind,
                             int y) {
  const auto label = dashboard_row_label(kind);
  const SDL_Rect chip{18, y, text_width(label, 2) + 28, 28};
  fill_rect(renderer, chip, dashboard_row_color(kind));
  draw_centered_text(renderer, label, chip.x + chip.w / 2, chip.y + 5, 2,
                     kText);
}

void draw_dashboard_selected_title(SDL_Renderer* renderer,
                                   const GameDashboardPresentation& dashboard,
                                   const DashboardRow& row, int y) {
  if (row.games.empty()) return;
  const auto focus = std::min(dashboard.item_focus(row.kind), row.games.size() - 1);
  const auto* game = dashboard.find_game(row.games[focus].item_id);
  if (game == nullptr) return;
  std::string game_title = game->inventory.title;
  if (const auto parenthesis = game_title.find('(');
      parenthesis != std::string::npos) {
    game_title.erase(parenthesis);
  }
  std::string normalized_title;
  bool previous_was_space = false;
  for (const char character : game_title) {
    if (character == ' ') {
      if (!normalized_title.empty() && !previous_was_space) {
        normalized_title.push_back(character);
      }
      previous_was_space = true;
    } else {
      normalized_title.push_back(character);
      previous_was_space = false;
    }
  }
  game_title = std::move(normalized_title);
  const std::string suffix = "  .  " +
      std::string(game_platform_short_label(game->inventory.platform));
  std::string title = game_title + suffix;
  const SDL_Rect chip{kWidth - text_width(title, 2) - 32, y,
                      text_width(title, 2) + 28, 28};
  fill_rect(renderer, chip, dashboard_row_color(row.kind));
  draw_centered_text(renderer, title, chip.x + chip.w / 2, chip.y + 5, 2, kText);
}

void draw_dashboard_selected_setting(SDL_Renderer* renderer,
                                    const DashboardRow& row, std::size_t focus,
                                    int y) {
  if (row.settings.empty()) return;
  const auto& label = row.settings[std::min(focus, row.settings.size() - 1)];
  const SDL_Rect chip{kWidth - text_width(label, 2) - 32, y,
                      text_width(label, 2) + 28, 28};
  fill_rect(renderer, chip, dashboard_row_color(row.kind));
  draw_centered_text(renderer, label, chip.x + chip.w / 2, chip.y + 5, 2, kText);
}

void render_dashboard_game_card(SDL_Renderer* renderer,
                                const GameDashboardPresentation& dashboard,
                                const DashboardGameCard& card, SDL_Rect bounds,
                                bool focused) {
  const auto* game = dashboard.find_game(card.item_id);
  if (game == nullptr) return;
  // The offset shadow was designed for floating rounded tiles.  With square
  // edges it reads as a stray background strip, so remove it entirely.
  if (gRoundedTiles) {
    fill_rect(renderer, {bounds.x + 5, bounds.y + 6, bounds.w, bounds.h},
              {32, 55, 45, 82});
  }
  std::filesystem::path artwork = game->inventory.artwork_path;
  if (!artwork.empty() && artwork.is_relative()) {
    artwork = executable_asset(artwork.generic_string());
  }
  SDL_Texture* texture = artwork.empty() ? nullptr : cached_texture(renderer, artwork);
  if (texture != nullptr) {
    render_rounded_artwork(renderer, texture, bounds, gRoundedTiles ? 14 : 0);
  } else {
    const auto seed = static_cast<std::uint32_t>(
        std::hash<std::string>{}(game->inventory.item_id));
    fill_rect(renderer, bounds,
              {static_cast<std::uint8_t>(76 + seed % 70),
               static_cast<std::uint8_t>(98 + (seed >> 8U) % 70),
               static_cast<std::uint8_t>(115 + (seed >> 16U) % 70)});
    draw_centered_text(renderer, game->inventory.title.substr(0, 18),
                       bounds.x + bounds.w / 2, bounds.y + bounds.h / 2 - 8,
                       2, {255, 250, 231});
  }
  const SDL_Rect review_badge{bounds.x + bounds.w - 39, bounds.y + 10, 29, 29};
  fill_rect(renderer, review_badge, {255, 250, 231, 205});
  draw_review_icon(renderer, game->profile.verdict,
                   review_badge.x + 6, review_badge.y + 6);
  if (focused) outline_rect(renderer, bounds, 4, kFocus);
}

void render_dashboard_background(SDL_Renderer* renderer,
                                 const GameDashboardPresentation& dashboard) {
  constexpr std::string_view prefix = "builtin:";
  const auto reference = dashboard.profile_background_ref();
  if (starts_with(reference, prefix)) {
    if (const auto* background = find_built_in_background(
            reference.substr(prefix.size())); background != nullptr) {
      if (background->flat_color) {
        SDL_SetRenderDrawColor(
            renderer, static_cast<std::uint8_t>((background->color_rgb >> 16) & 0xffU),
            static_cast<std::uint8_t>((background->color_rgb >> 8) & 0xffU),
            static_cast<std::uint8_t>(background->color_rgb & 0xffU), 255);
        SDL_RenderClear(renderer);
        return;
      }
      render_storybook_background(
          renderer, executable_asset("backgrounds/" +
                                     std::string(background->filename)));
      return;
    }
  }
  render_storybook_background(renderer);
}

void render_dashboard_filters(SDL_Renderer* renderer,
                              const GameDashboardPresentation& dashboard) {
  render_dashboard_background(renderer, dashboard);
  constexpr std::array<std::string_view, 5> labels{
      "SYSTEM", "REVIEW", "PLAY", "FAMILY", "SHOW"};
  // Lucide's standard list-filter glyph; no title bar or control legend.
  if (SDL_Texture* filter_icon = cached_texture(
          renderer, executable_asset("icons/filter.png"));
      filter_icon != nullptr) {
    SDL_SetTextureColorMod(filter_icon, kText.red, kText.green, kText.blue);
    SDL_SetTextureAlphaMod(filter_icon, kText.alpha);
    const SDL_Rect destination{16, 14, 50, 50};
    SDL_RenderCopy(renderer, filter_icon, nullptr, &destination);
  }
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const bool focused = index == static_cast<std::size_t>(dashboard.filter_category());
    const SDL_Rect tab{78 + static_cast<int>(index) * 108, 14, 100, 52};
    fill_rect(renderer, tab, focused ? kPanelFocused : kPanel);
    if (focused) outline_rect(renderer, tab, 4, kFocus);
    draw_centered_text(renderer, labels[index], tab.x + tab.w / 2, tab.y + 17,
                       2, focused ? kText : kMuted);
  }

  std::vector<std::string> options;
  switch (dashboard.filter_category()) {
    case GameFilterCategory::Platform:
      options.push_back("ALL");
      for (const auto& platform : dashboard.available_platforms()) {
        options.emplace_back(game_platform_short_label(platform.platform));
      }
      break;
    case GameFilterCategory::Review:
      options = {"ALL", "?", "+", "-"};
      break;
    case GameFilterCategory::Progress:
      options = {"ALL", "PLAYING", "COMPLETE"};
      break;
    case GameFilterCategory::Family:
      options = {"ALL", "RECOMMENDED", "FOR KIDS"};
      for (const auto& child : dashboard.child_profiles()) options.push_back(child.display_name);
      break;
    case GameFilterCategory::Visibility:
      options = {"CURRENT", "HIDDEN"};
      break;
  }
  const auto focus = dashboard.filter_option_focus();
  const bool platform_category =
      dashboard.filter_category() == GameFilterCategory::Platform;
  const std::size_t visible_options = platform_category ? 4 : 6;
  const int option_height = platform_category ? 76 : 50;
  const int option_gap = platform_category ? 10 : 8;
  const std::size_t first = focus < visible_options ? 0 : focus - visible_options + 1;
  for (std::size_t index = first;
       index < std::min(options.size(), first + visible_options); ++index) {
    const SDL_Rect option{48,
                          88 + static_cast<int>(index - first) *
                                   (option_height + option_gap),
                          544, option_height};
    const bool selected = index == focus;
    fill_rect(renderer, option, selected ? kPanelFocused : kPanel);
    if (selected) outline_rect(renderer, option, 4, kFocus);
    const bool platform_option = dashboard.filter_category() == GameFilterCategory::Platform &&
                                 index > 0 && index - 1 < dashboard.available_platforms().size();
    if (platform_option) {
      draw_platform_icon(renderer, dashboard.available_platforms()[index - 1].platform,
                         option.x + 22, option.y + 17, selected ? kText : kMuted,
                         42);
    }
    draw_text(renderer, options[index].substr(0, 26),
              option.x + (platform_option ? 84 : 22),
              option.y + (platform_option ? 27 : 14),
              platform_option ? 3 : 2, selected ? kText : kMuted);
  }
  present_frame(renderer);
}

void render_dashboard_search(SDL_Renderer* renderer,
                             const GameDashboardPresentation& dashboard) {
  render_dashboard_background(renderer, dashboard);
  fill_rect(renderer, {18, 14, 604, 56}, kPanel);
  draw_text(renderer, "SEARCH", 32, 33, 2, kMuted);
  const std::string query = dashboard.search_query().empty() ? "_" : std::string(dashboard.search_query()) + "_";
  draw_text(renderer, query.substr(0, 34), 146, 28, 3, kText);
  std::string filters = dashboard.filter().active() ? "FILTERED" : "ALL GAMES";
  fill_rect(renderer, {18, 76, text_width(filters, 1) + 24, 22}, dashboard_row_color(DashboardRowKind::Platforms));
  draw_text(renderer, filters, 30, 82, 1, kText);
  const auto results = dashboard.search_results();
  constexpr std::size_t visible = 3;
  const std::size_t first = dashboard.search_result_focus() < visible ? 0 : dashboard.search_result_focus() - visible + 1;
  for (std::size_t index = first; index < std::min(results.size(), first + visible); ++index) {
    const SDL_Rect row{18, 106 + static_cast<int>(index - first) * 42, 604, 34};
    const bool focused = dashboard.search_results_focused() && index == dashboard.search_result_focus();
    fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
    if (focused) outline_rect(renderer, row, 3, kFocus);
    draw_text(renderer, results[index].inventory.title.substr(0, 31), 34, row.y + 10, 2, focused ? kText : kMuted);
    draw_platform_icon(renderer, results[index].inventory.platform, 578, row.y + 7, focused ? kText : kMuted);
  }
  constexpr std::string_view keys = "QWERTYUIOPASDFGHJKLZXCVBNM<_";
  constexpr int columns = 10;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const int column = static_cast<int>(index % columns);
    const int row = static_cast<int>(index / columns);
    const SDL_Rect key{18 + column * 61, 258 + row * 58, 54, 48};
    const bool focused = !dashboard.search_results_focused() && index == dashboard.search_keyboard_focus();
    fill_rect(renderer, key, focused ? kPanelFocused : kPanel);
    if (focused) outline_rect(renderer, key, 3, kFocus);
    const std::string label = keys[index] == '<' ? "<" : keys[index] == '_' ? "SPACE" : std::string(1, keys[index]);
    draw_centered_text(renderer, label, key.x + key.w / 2, key.y + 15, keys[index] == '_' ? 1 : 2, focused ? kText : kMuted);
  }
  present_frame(renderer);
}

void render_dashboard_details(SDL_Renderer* renderer,
                              const GameDashboardPresentation& dashboard) {
  render_dashboard_background(renderer, dashboard);
  const auto* game = dashboard.selected_game();
  if (game == nullptr) {
    draw_heading_panel(renderer, {72, 20, 496, 80}, "GAME", "NOT AVAILABLE");
    draw_scroll_right_hint(renderer);
    present_frame(renderer);
    return;
  }
  fill_rect(renderer, {18, 12, 604, 52}, {255, 250, 231, 240});
  draw_text(renderer, game->inventory.title.substr(0, 30), 32, 23, 3, kText);
  draw_platform_icon(renderer, game->inventory.platform, 590, 28, kMuted);
  constexpr std::array<std::string_view, 3> tabs{"OVERVIEW", "REVIEW", "FAMILY"};
  for (std::size_t index = 0; index < tabs.size(); ++index) {
    const bool active = index == static_cast<std::size_t>(dashboard.detail_page());
    const SDL_Rect tab{82 + static_cast<int>(index) * 160, 78, 150, 30};
    fill_rect(renderer, tab, active ? kPanelFocused : kPanel);
    if (active) outline_rect(renderer, tab, 3, kFocus);
    draw_centered_text(renderer, tabs[index], tab.x + tab.w / 2, tab.y + 10, 1,
                       active ? kText : kMuted);
  }
  if (dashboard.detail_page() == GameDetailPage::Overview) {
    SDL_Rect artwork{34, 126, 274, 224};
    fill_rect(renderer, artwork, {93, 127, 110});
    std::filesystem::path path = game->inventory.artwork_path;
    if (dashboard.detail_artwork_focus() > 0 &&
        dashboard.detail_artwork_focus() <=
            game->inventory.screenshot_paths.size()) {
      path = game->inventory.screenshot_paths[
          dashboard.detail_artwork_focus() - 1];
    }
    if (!path.empty() && path.is_relative()) path = executable_asset(path.generic_string());
    if (auto* texture = path.empty() ? nullptr : cached_texture(renderer, path); texture != nullptr) {
      SDL_RenderCopy(renderer, texture, nullptr, &artwork);
    }
    fill_rect(renderer, {330, 126, 276, 224}, kPanel);
    draw_text(renderer, std::string(game_platform_name(game->inventory.platform)), 350, 148, 2, kText);
    draw_text(renderer, compact_time(game->play.active_milliseconds), 350, 190, 4, kFocus);
    draw_text(renderer, "PLAYED", 350, 232, 1, kMuted);
    draw_review_icon(renderer, game->profile.verdict, 352, 272);
    draw_trophy_icon(renderer, 396, 272, game->profile.completed);
    draw_centered_text(renderer, "A PLAY", 468, 320, 2, kText);
    if (!game->inventory.screenshot_paths.empty()) {
      draw_centered_text(
          renderer,
          std::to_string(dashboard.detail_artwork_focus() + 1) + "/" +
              std::to_string(game->inventory.screenshot_paths.size() + 1),
          artwork.x + artwork.w / 2, 360, 1, kMuted);
    }
  } else {
    std::vector<std::pair<std::string, bool>> choices;
    if (dashboard.detail_page() == GameDetailPage::Review) {
      choices = {{"THUMBS UP", game->profile.verdict == GameReviewVerdict::Positive},
                 {"THUMBS DOWN", game->profile.verdict == GameReviewVerdict::Negative},
                 {"COMPLETE", game->profile.completed}};
    } else {
      choices = {{"RECOMMENDED", game->household.recommended},
                 {"FOR KIDS", game->household.for_kids},
                 {"HIDDEN", game->household.hidden}};
      for (const auto& child : dashboard.child_profiles()) {
        const bool assigned = std::find(game->child_allowed_profile_ids.begin(),
                                        game->child_allowed_profile_ids.end(),
                                        child.id) != game->child_allowed_profile_ids.end();
        choices.emplace_back(child.display_name, assigned);
      }
    }
    for (std::size_t index = 0; index < choices.size(); ++index) {
      const SDL_Rect row{106, 136 + static_cast<int>(index) * 44, 428, 36};
      const bool focused = index == dashboard.detail_focus();
      fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
      if (focused) outline_rect(renderer, row, 3, kFocus);
      draw_text(renderer, choices[index].first.substr(0, 26), row.x + 20,
                row.y + 12, 2, kText);
      draw_text(renderer, choices[index].second ? "ON" : "-", row.x + row.w - 52,
                row.y + 12, 2, choices[index].second ? kFocus : kMuted);
    }
  }
  if (!dashboard.notice().empty()) {
    draw_centered_text(renderer, dashboard.notice(), kWidth / 2, 410, 2, kFocus);
  }
  draw_footer(renderer, "L R PAGE   A SELECT   B BACK");
  present_frame(renderer);
}

}  // namespace

void render_game_dashboard(SDL_Renderer* renderer,
                           const GameDashboardPresentation& dashboard) {
  apply_interface_theme(dashboard.interface_theme());
  apply_accent(dashboard.accent_rgb());
  gRoundedTiles = dashboard.rounded_tiles();
  if (dashboard.stage() == GameDashboardStage::Filters) {
    render_dashboard_filters(renderer, dashboard);
    return;
  }
  if (dashboard.stage() == GameDashboardStage::Search) {
    render_dashboard_search(renderer, dashboard);
    return;
  }
  if (dashboard.stage() == GameDashboardStage::Details) {
    render_dashboard_details(renderer, dashboard);
    return;
  }
  render_dashboard_background(renderer, dashboard);
  // Keep the dashboard's viewport stable.  SDL's renderer on the device cannot
  // safely animate these clipped, texture-backed rows by translating the final
  // layout; that path can leave the display empty after a row navigation.
  const int row_motion = 0;
  const auto rows = dashboard.rows();
  if (rows.empty()) {
    draw_heading_panel(renderer, {90, 174, 460, 112}, "NO MATCHES");
  } else {
    const auto row_focus = dashboard.row_focus();
    const bool big_mode = dashboard.big_mode();
    const std::size_t visible_rows = big_mode ? 1U : 2U;
    const std::size_t first = big_mode
                                  ? row_focus
                                  : (rows.size() <= 2 ? 0
                                                      : std::min(row_focus, rows.size() - 2));
    for (std::size_t row_index = first;
         row_index < std::min(rows.size(), first + visible_rows); ++row_index) {
      const auto& row = rows[row_index];
      const bool focused_row = row_index == row_focus;
      const int row_slot = static_cast<int>(row_index - first);
      const int row_height = big_mode ? 408 : 216;
      const int card_height = big_mode ? 364 : 174;
      const int title_y = 4 + row_slot * row_height + row_motion;
      const int card_y = title_y + 34;
      draw_dashboard_row_chip(renderer, row.kind, title_y);
      if (focused_row) {
        if (!row.settings.empty()) {
          draw_dashboard_selected_setting(renderer, row, dashboard.item_focus(row.kind), title_y);
        } else {
          draw_dashboard_selected_title(renderer, dashboard, row, title_y);
        }
      }
      if (!row.games.empty()) {
        const auto focus = std::min(dashboard.item_focus(row.kind), row.games.size() - 1);
        std::size_t index = focus;
          int card_x = 18;
        while (index < row.games.size() && card_x < kWidth - 18) {
          const int card_width = dashboard_card_width(renderer, dashboard,
                                                      row.games[index], card_height);
          render_dashboard_game_card(renderer, dashboard, row.games[index],
                                     {card_x, card_y, card_width, card_height},
                                     focused_row && index == focus);
          card_x += card_width + 12;
          ++index;
        }
      } else if (!row.platforms.empty()) {
        const auto focus = std::min(dashboard.item_focus(row.kind), row.platforms.size() - 1);
        const std::size_t platform_first = focus < 3 ? 0 : focus - 2;
        for (std::size_t index = platform_first;
             index < std::min(row.platforms.size(), platform_first + 3); ++index) {
          const SDL_Rect card{18 + static_cast<int>(index - platform_first) * 206,
                              card_y, 194, 174};
          const bool selected = focused_row && index == focus;
          fill_rect(renderer, card, selected ? kPanelFocused : kPanel);
          if (selected) outline_rect(renderer, card, 4, kFocus);
          // Platform art is the content of this row.  Keep its canvas nearly
          // card-sized: the wordmark/count are deliberately only a small
          // footer rather than competing with the icon.  The art follows the
          // profile accent, so it belongs to the active Sprout theme instead
          // of inheriting the source logo's near-black ink.
          draw_platform_icon(renderer, row.platforms[index].platform,
                             card.x + card.w / 2 - 66, card.y + 5, kFocus, 132);
          draw_centered_text(renderer,
                             game_platform_short_label(row.platforms[index].platform),
                             card.x + card.w / 2, card.y + 143, 1, kText);
          draw_centered_text(renderer, std::to_string(row.platforms[index].game_count),
                             card.x + card.w / 2, card.y + 157, 1, kMuted);
        }
      } else if (row.statistics.has_value()) {
        const auto& stats = *row.statistics;
        const std::array<std::pair<std::string, std::string>, 4> cards{{
            {"CURATED", review_percent(stats.recommended_reviewed,
                                        stats.recommended_total)},
            {"REVIEWED", review_percent(stats.library_reviewed,
                                         stats.library_total)},
            {"OUTSIDE", std::to_string(stats.outside_recommended_reviewed)},
            {"WON", std::to_string(stats.completed)},
        }};
        const auto focus = std::min(dashboard.item_focus(row.kind), cards.size() - 1);
        const std::size_t card_first = focus < 3 ? 0 : focus - 2;
        for (std::size_t index = card_first;
             index < std::min(cards.size(), card_first + 3); ++index) {
          const SDL_Rect card{18 + static_cast<int>(index - card_first) * 206,
                              card_y, 194, 174};
          const bool selected = focused_row && dashboard.item_focus(row.kind) == index;
          fill_rect(renderer, card, selected ? kPanelFocused : kPanel);
          if (selected) outline_rect(renderer, card, 4, kFocus);
          draw_centered_text(renderer, cards[index].second, card.x + card.w / 2,
                             card.y + 42, 6, kFocus);
          draw_centered_text(renderer, cards[index].first, card.x + card.w / 2,
                             card.y + 121, 2, kMuted);
        }
      } else if (!row.settings.empty()) {
        const auto focus = std::min(dashboard.item_focus(row.kind), row.settings.size() - 1);
        // Settings behave as a true queue: moving focus advances the next
        // card into the leftmost, active position every time.
        const std::size_t first_setting = focus;
        for (std::size_t index = first_setting;
             index < std::min(row.settings.size(), first_setting + 3); ++index) {
          const SDL_Rect card{18 + static_cast<int>(index - first_setting) * 206,
                              card_y, 194, 174};
          const bool selected = focused_row && index == focus;
          const bool enabled = row.settings[index] == "BIG MODE" && dashboard.big_mode();
          fill_rect(renderer, card, selected ? kPanelFocused
                                             : (enabled ? Color{255, 239, 188, 240}
                                                        : kPanel));
          if (selected) outline_rect(renderer, card, 4, kFocus);
          else if (enabled) outline_rect(renderer, card, 3, kHoney);
          if (row.settings[index] == "PROFILE") {
            constexpr std::string_view prefix = "builtin:";
            const auto reference = dashboard.profile_avatar_ref();
            if (starts_with(reference, prefix)) {
              if (const auto* avatar = find_built_in_avatar(
                      reference.substr(prefix.size())); avatar != nullptr) {
                try {
                  const SDL_Rect portrait{card.x + 28, card.y + 18,
                                          card.w - 56, card.h - 36};
                  static_cast<void>(render_treated_portrait(
                      renderer, executable_asset("avatars/thumbs/" +
                                                 std::string(avatar->id) + ".png"),
                      portrait, selected ? kFocus : kHoney, selected ? 6.0F : 4.0F));
                } catch (const std::exception&) {
                }
              }
            }
          } else if (row.settings[index] == "BACKGROUND") {
            constexpr std::string_view prefix = "builtin:";
            const auto reference = dashboard.profile_background_ref();
            if (starts_with(reference, prefix)) {
              if (const auto* background = find_built_in_background(
                      reference.substr(prefix.size())); background != nullptr) {
                const SDL_Rect scene{card.x + 8, card.y + 8,
                                     card.w - 16, card.h - 16};
                if (background->flat_color) {
                  fill_rect(renderer, scene,
                            {static_cast<std::uint8_t>((background->color_rgb >> 16) & 0xffU),
                             static_cast<std::uint8_t>((background->color_rgb >> 8) & 0xffU),
                             static_cast<std::uint8_t>(background->color_rgb & 0xffU), 255});
                } else if (SDL_Texture* texture = cached_texture(
                        renderer, executable_asset("backgrounds/" +
                                                   std::string(background->filename)));
                    texture != nullptr) {
                  SDL_RenderCopy(renderer, texture, nullptr, &scene);
                }
              }
            }
          } else if (row.settings[index] == "BIG MODE") {
            const Color glyph = dashboard.big_mode() ? kFocus : kMuted;
            const auto asset = dashboard.big_mode() ? "icons/big-mode.png"
                                                    : "icons/small-mode.png";
            if (SDL_Texture* icon = cached_texture(renderer, executable_asset(asset));
                icon != nullptr) {
              SDL_SetTextureColorMod(icon, glyph.red, glyph.green, glyph.blue);
              SDL_SetTextureAlphaMod(icon, glyph.alpha);
              const SDL_Rect destination{card.x + 45, card.y + 28,
                                         card.w - 90, card.h - 56};
              SDL_RenderCopy(renderer, icon, nullptr, &destination);
            } else {
              const SDL_Rect screen{card.x + 38, card.y + 38, card.w - 76,
                                    card.h - 76};
              outline_rect(renderer, screen, 3, glyph);
            }
          } else if (row.settings[index] == "TILE STYLE") {
            const SDL_Rect sample{card.x + 42, card.y + 36, card.w - 84, card.h - 72};
            const bool rounded = dashboard.rounded_tiles();
            const bool saved = gRoundedTiles;
            gRoundedTiles = rounded;
            fill_rect(renderer, sample, kFocus);
            outline_rect(renderer, sample, 3, kText);
            gRoundedTiles = saved;
          } else if (row.settings[index] == "MOTION") {
            const SDL_Rect trail{card.x + 34, card.y + 78, card.w - 68, 10};
            fill_rect(renderer, trail, kPanelFocused);
            fill_rect(renderer, {trail.x, trail.y, dashboard.motion_enabled() ? trail.w : trail.w / 3, trail.h}, kFocus);
          } else if (row.settings[index] == "THEME") {
            if (SDL_Texture* icon = cached_texture(renderer, executable_asset("icons/theme.png"));
                icon != nullptr) {
              SDL_SetTextureColorMod(icon, kFocus.red, kFocus.green, kFocus.blue);
              SDL_SetTextureAlphaMod(icon, 255);
              const SDL_Rect destination{card.x + 46, card.y + 30, 102, 102};
              SDL_RenderCopy(renderer, icon, nullptr, &destination);
            }
          } else if (row.settings[index] == "ACCENT") {
            const Color accent{static_cast<std::uint8_t>((dashboard.accent_rgb() >> 16) & 0xffU),
                               static_cast<std::uint8_t>((dashboard.accent_rgb() >> 8) & 0xffU),
                               static_cast<std::uint8_t>(dashboard.accent_rgb() & 0xffU)};
            if (SDL_Texture* icon = cached_texture(renderer, executable_asset("icons/accent.png"));
                icon != nullptr) {
              SDL_SetTextureColorMod(icon, accent.red, accent.green, accent.blue);
              SDL_SetTextureAlphaMod(icon, 255);
              const SDL_Rect destination{card.x + 46, card.y + 30, 102, 102};
              SDL_RenderCopy(renderer, icon, nullptr, &destination);
            }
          } else {
            draw_centered_text(renderer, row.settings[index], card.x + card.w / 2,
                               card.y + 68, 2, kText);
          }
        }
      }
    }
    const auto preview_index = first + visible_rows;
    if (preview_index < rows.size()) {
      draw_dashboard_row_chip(renderer, rows[preview_index].kind, 448);
    }
  }
  if (!dashboard.notice().empty()) {
    const SDL_Rect notice{180, 442, 280, 30};
    fill_rect(renderer, notice, {255, 244, 196, 236});
    draw_centered_text(renderer, dashboard.notice(), kWidth / 2, 450, 1, kFocus);
  }
  present_frame(renderer);
}

void render_profile_archive(SDL_Renderer* renderer,
                            const ProfileArchivePresentation& archive) {
  render_storybook_background(renderer);

  draw_centered_text(renderer, archive.title(), kWidth / 2, 30, 3, kText);

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
  present_frame(renderer);
}

void render_profile_avatars(
    SDL_Renderer* renderer, const ProfileAvatarPresentation& presentation,
    const std::filesystem::path& built_in_avatar_root) {
  const auto render_configured_background = [&] {
    const auto* profile = presentation.selected_profile();
    constexpr std::string_view prefix = "builtin:";
    if (profile != nullptr && starts_with(profile->background_ref, prefix)) {
      if (const auto* background = find_built_in_background(
              std::string_view(profile->background_ref).substr(prefix.size()));
          background != nullptr) {
        if (background->flat_color) {
          SDL_SetRenderDrawColor(renderer,
              static_cast<std::uint8_t>((background->color_rgb >> 16) & 0xffU),
              static_cast<std::uint8_t>((background->color_rgb >> 8) & 0xffU),
              static_cast<std::uint8_t>(background->color_rgb & 0xffU), 255);
          SDL_RenderClear(renderer);
          return;
        }
        if (SDL_Texture* texture = cached_texture(
                renderer, executable_asset("backgrounds/" +
                    std::string(background->filename))); texture != nullptr) {
          const SDL_Rect canvas{0, 0, kWidth, kHeight};
          SDL_RenderCopy(renderer, texture, nullptr, &canvas);
          return;
        }
      }
    }
    render_storybook_background(renderer);
  };
  if (presentation.stage() == ProfileAvatarStage::Accent) {
    constexpr std::array<Color, 30> accents{{
        {229,117,87},{240,126,72},{240,167,76},{228,204,85},{191,203,89},
        {141,190,91},{82,182,108},{82,182,161},{78,182,191},{85,166,217},
        {77,130,214},{105,118,216},{138,120,214},{155,120,208},{184,117,196},
        {210,116,172},{216,111,135},{200,93,93},{168,78,85},{129,75,86},
        {245,241,230},{221,213,199},{191,195,200},{147,155,165},{112,121,133},
        {66,72,80},{45,50,57},{255,255,255},{18,22,28},{142,110,77}}};
    render_configured_background();
    const Color selected = accents[presentation.focus_index()];
    fill_rect(renderer, {84, 26, 472, 150}, kPanel);
    outline_rect(renderer, {106, 48, 428, 106}, 6, selected);
    fill_rect(renderer, {128, 76, 384, 52}, kPanelFocused);
    outline_rect(renderer, {128, 76, 384, 52}, 4, selected);
    constexpr std::size_t columns = 10;
    const auto focus = presentation.focus_index();
    for (std::size_t index = 0; index < accents.size(); ++index) {
      const int column = static_cast<int>(index % columns);
      const int row = static_cast<int>(index / columns);
      const SDL_Rect tile{35 + column * 58, 220 + row * 72, 46, 56};
      fill_rect(renderer, tile, accents[index]);
      if (index == focus) outline_rect(renderer, tile, 5, kText);
    }
    present_frame(renderer);
    return;
  }
  if (presentation.stage() == ProfileAvatarStage::Background) {
    const auto backgrounds = presentation.backgrounds();
    if (!backgrounds.empty()) {
      const auto& background = backgrounds[presentation.focus_index()];
      if (background.flat_color) {
        SDL_SetRenderDrawColor(
            renderer, static_cast<std::uint8_t>((background.color_rgb >> 16) & 0xffU),
            static_cast<std::uint8_t>((background.color_rgb >> 8) & 0xffU),
            static_cast<std::uint8_t>(background.color_rgb & 0xffU), 255);
        SDL_RenderClear(renderer);
      } else if (SDL_Texture* texture = cached_texture(
                     renderer, executable_asset("backgrounds/" +
                                                std::string(background.filename)));
                 texture != nullptr) {
        const SDL_Rect canvas{0, 0, kWidth, kHeight};
        SDL_RenderCopy(renderer, texture, nullptr, &canvas);
      }

      // The focused choice owns the full screen; tiles are only the picker.
      fill_rect(renderer, {0, 232, kWidth, kHeight - 232}, {12, 25, 35, 132});
      constexpr std::size_t row_count = 2;
      const std::size_t total = backgrounds.size();
      const std::size_t column_count = (total + row_count - 1U) / row_count;
      const std::size_t focus = presentation.focus_index();
      const std::size_t focus_column = focus / row_count;
      for (int relative_column = -1; relative_column <= 1; ++relative_column) {
        const auto column = static_cast<std::size_t>(
            (static_cast<long long>(focus_column) + relative_column +
             static_cast<long long>(column_count)) %
            static_cast<long long>(column_count));
        for (std::size_t row = 0; row < row_count; ++row) {
          const std::size_t index = column * row_count + row;
          if (index >= total) continue;
          const auto& choice = backgrounds[index];
          const bool selected = index == focus;
          const SDL_Rect tile{112 + (relative_column + 1) * 144,
                              250 + static_cast<int>(row) * 108, 128, 92};
          fill_rect(renderer, tile, {255, 250, 231, 224});
          const SDL_Rect art{tile.x + 5, tile.y + 5, tile.w - 10, tile.h - 10};
          if (choice.flat_color) {
            fill_rect(renderer, art,
                      {static_cast<std::uint8_t>((choice.color_rgb >> 16) & 0xffU),
                       static_cast<std::uint8_t>((choice.color_rgb >> 8) & 0xffU),
                       static_cast<std::uint8_t>(choice.color_rgb & 0xffU), 255});
          } else if (SDL_Texture* texture = cached_texture(
                         renderer, executable_asset("backgrounds/" +
                                                    std::string(choice.filename)));
                     texture != nullptr) {
            SDL_RenderCopy(renderer, texture, nullptr, &art);
          }
          if (selected) outline_rect(renderer, tile, 5, kFocus);
        }
      }
    }
    draw_scroll_right_hint(renderer);
    present_frame(renderer);
    return;
  }

  if (presentation.stage() == ProfileAvatarStage::Avatar) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    const auto avatars = presentation.avatars();
    const std::size_t total = avatars.size() +
                              (presentation.custom_image_available() ? 1U : 0U);
    if (total != 0) {
      constexpr std::size_t row_count = 3;
      const std::size_t column_count = (total + row_count - 1U) / row_count;
      const std::size_t focus = presentation.focus_index();
      const std::size_t first_column = presentation.avatar_first_column();
      for (std::size_t visible_column = 0; visible_column < 3; ++visible_column) {
        const auto column = first_column + visible_column;
        if (column >= column_count) break;
        for (std::size_t row = 0; row < row_count; ++row) {
          const std::size_t index = column * row_count + row;
          if (index >= total) continue;
          const bool selected = index == focus;
          const int size = selected ? 118 : 98;
          const int center_x = 136 + static_cast<int>(visible_column) * 184;
          const int center_y = 92 + static_cast<int>(row) * 150;
          const SDL_Rect cell{center_x - size / 2, center_y - size / 2,
                              size, size};
          if (index < avatars.size()) {
            try {
              static_cast<void>(render_treated_portrait(
                  renderer,
                  built_in_avatar_thumbnail_path(built_in_avatar_root,
                                                 avatars[index].id),
                  cell, selected ? kFocus : Color{255, 250, 231},
                  selected ? 6.0F : 3.0F));
            } catch (const std::exception&) {
              fill_rect(renderer, cell, kPanel);
            }
          } else {
            fill_rect(renderer, cell, kPanel);
            draw_centered_text(renderer, "+", center_x, center_y - 28, 8,
                               selected ? kFocus : kMuted);
          }
        }
      }
    }
    draw_scroll_right_hint(renderer);
    present_frame(renderer);
    return;
  }

  render_configured_background();

  if (presentation.stage() == ProfileAvatarStage::Profile) {
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
  } else {
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
  present_frame(renderer);
}

void render_profile_image_crop(SDL_Renderer* renderer,
                               const ProfileImageCropPresentation& crop) {
  render_storybook_background(renderer);
  draw_centered_text(renderer, "CROP", kWidth / 2, 28, 3, kText);

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
  present_frame(renderer);
}

void render_parent_pin(SDL_Renderer* renderer,
                       const ParentPinPresentation& pin) {
  // PIN entry is deliberately neutral: this is a focused security flow, not
  // another dashboard card over a potentially busy profile illustration.
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  const bool combo = pin.uses_button_combo();
  draw_centered_text(renderer, combo ? "ENTER PIN" : pin.title(), kWidth / 2, 54, 4, kText);
  if (!combo) draw_centered_text(renderer, pin.description(), kWidth / 2, 98, 1, kMuted);

  if (pin.is_confirmation()) {
    const SDL_Rect panel{126, 132, 388, 174};
    fill_rect(renderer, panel, kPanel);
    outline_rect(renderer, panel, 4, kFocus);
    draw_centered_text(renderer, "PIN SAVED", kWidth / 2, 182, 3, kFocus);
    draw_centered_text(renderer, "PARENT PROFILE PROTECTED", kWidth / 2, 236, 1, kMuted);
    present_frame(renderer);
    return;
  }
  if (pin.uses_button_combo()) {
    if (pin.ready_to_save()) {
      draw_centered_text(renderer, "COMBINATION READY", kWidth / 2, 206, 3, kFocus);
      draw_centered_text(renderer, "A SAVE", kWidth / 2, 284, 2, kText);
      draw_centered_text(renderer, "B CANCEL", kWidth / 2, 322, 1, kMuted);
      present_frame(renderer);
      return;
    }
    const auto count = pin.entered_digits();
    for (int index = 0; index < 4; ++index) {
      const SDL_Rect box{164 + index * 82, 142, 62, 70};
      fill_rect(renderer, box, kPanel);
      outline_rect(renderer, box, 3, index < static_cast<int>(count) ? kFocus : kPanelFocused);
      if (index < static_cast<int>(count)) draw_centered_text(renderer, "*", box.x + box.w / 2, 157, 4, kFocus);
    }
    fill_rect(renderer, {0, kHeight - 5,
                         static_cast<int>(kWidth * pin.expiry_fraction()), 5}, kFocus);
    if (!pin.error_message().empty()) draw_centered_text(renderer, pin.error_message(), kWidth / 2, 370, 1, kFocus);
    present_frame(renderer);
    return;
  }

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
