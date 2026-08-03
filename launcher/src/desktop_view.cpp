#include "desktop_view.hpp"
#include "sprout/launcher/built_in_avatar.hpp"
#include "sprout/launcher/portrait_outline.hpp"
#include "sprout/launcher/profile_image_importer.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sprout::launcher {
namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;

struct Color {
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::uint8_t alpha{255};
};

constexpr Color kBackground{18, 36, 32};
constexpr Color kPanel{31, 55, 49};
constexpr Color kPanelFocused{53, 82, 71};
constexpr Color kText{241, 238, 221};
constexpr Color kMuted{166, 184, 168};
constexpr Color kFocus{245, 199, 93};

void set_color(SDL_Renderer* renderer, Color color) {
  SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

void fill_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Color color) {
  set_color(renderer, color);
  SDL_RenderFillRect(renderer, &rect);
}

void outline_rect(SDL_Renderer* renderer, SDL_Rect rect, int thickness, Color color) {
  set_color(renderer, color);
  for (int index = 0; index < thickness; ++index) {
    SDL_RenderDrawRect(renderer, &rect);
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

int text_width(std::string_view text, int scale) {
  if (text.empty()) {
    return 0;
  }
  return static_cast<int>(text.size()) * 6 * scale - scale;
}

void draw_text(SDL_Renderer* renderer, std::string_view text, int x, int y, int scale,
               Color color) {
  set_color(renderer, color);
  for (const char character : text) {
    const auto rows = glyph(character);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((rows[row] & (1U << (4 - column))) == 0) {
          continue;
        }
        const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    x += 6 * scale;
  }
}

void draw_centered_text(SDL_Renderer* renderer, std::string_view text, int center_x, int y,
                        int scale, Color color) {
  draw_text(renderer, text, center_x - text_width(text, scale) / 2, y, scale, color);
}

Color color_from_rgb(std::uint32_t rgb) {
  return {
      static_cast<std::uint8_t>((rgb >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((rgb >> 8U) & 0xFFU),
      static_cast<std::uint8_t>(rgb & 0xFFU),
  };
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

bool render_treated_portrait(SDL_Renderer* renderer,
                             const std::filesystem::path& path,
                             const SDL_Rect& destination, Color border,
                             float border_radius = 5.0F) {
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
    SDL_RenderCopy(renderer, cached->border_texture, nullptr, &destination);
    SDL_RenderCopy(renderer, cached->source_texture, nullptr, &destination);
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

  SDL_Texture* border_texture =
      SDL_CreateTextureFromSurface(renderer, border_surface);
  SDL_Texture* source_texture = SDL_CreateTextureFromSurface(renderer, source);
  SDL_FreeSurface(border_surface);
  SDL_FreeSurface(source);
  if (border_texture == nullptr || source_texture == nullptr) {
    if (border_texture != nullptr) SDL_DestroyTexture(border_texture);
    if (source_texture != nullptr) SDL_DestroyTexture(source_texture);
    return false;
  }
  SDL_SetTextureBlendMode(border_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_BLEND);
  SDL_RenderCopy(renderer, border_texture, nullptr, &destination);
  SDL_RenderCopy(renderer, source_texture, nullptr, &destination);
  cache.push_back({renderer, encoded_path, border, radius_tenths,
                   border_texture, source_texture});
  return true;
}

void render_profile_select(SDL_Renderer* renderer, const LauncherState& state,
                           const std::filesystem::path& managed_image_root,
                           const std::filesystem::path& built_in_avatar_root) {
  draw_centered_text(renderer, "SPROUT", kWidth / 2, 34, 5, kText);
  draw_centered_text(renderer, "WHO IS PLAYING?", kWidth / 2, 82, 2, kMuted);

  constexpr int card_width = 230;
  constexpr int card_height = 268;
  constexpr int gap = 24;
  const int total_width = static_cast<int>(state.profiles().size()) * card_width +
                          (static_cast<int>(state.profiles().size()) - 1) * gap;
  const int start_x = (kWidth - total_width) / 2;

  for (std::size_t index = 0; index < state.profiles().size(); ++index) {
    const auto& profile = state.profiles()[index];
    const int x = start_x + static_cast<int>(index) * (card_width + gap);
    const SDL_Rect card{x, 120, card_width, card_height};
    fill_rect(renderer, card, index == state.focus_index() ? kPanelFocused : kPanel);
    if (index == state.focus_index()) {
      outline_rect(renderer, card, 4, kFocus);
    }

    const SDL_Rect avatar{x + 55, 151, 120, 120};
    bool rendered_portrait = false;
    if (!managed_image_root.empty() &&
        starts_with(profile.avatar_ref, "local:")) {
      try {
        const auto path = ProfileImageImporter::resolve_portrait_at(
            managed_image_root, profile.avatar_ref);
        rendered_portrait = render_treated_portrait(
            renderer, path, avatar,
            index == state.focus_index() ? kFocus
                                         : color_from_rgb(profile.accent_rgb));
      } catch (const std::exception&) {
      }
    } else if (!built_in_avatar_root.empty()) {
      const auto* built_in = find_built_in_avatar(profile.avatar_ref);
      if (built_in != nullptr) {
        try {
          const auto path = built_in_avatar_thumbnail_path(
              built_in_avatar_root, built_in->id);
          rendered_portrait = render_treated_portrait(
              renderer, path, avatar,
              index == state.focus_index() ? kFocus
                                           : color_from_rgb(profile.accent_rgb));
        } catch (const std::exception&) {
        }
      }
    }
    if (!rendered_portrait) {
      const std::string initial(1, profile.display_name.front());
      draw_centered_text(renderer, initial, x + card_width / 2, 175, 8, kText);
    }

    draw_centered_text(renderer, profile.display_name, x + card_width / 2, 300, 3, kText);
    draw_centered_text(renderer,
                       profile.role == ProfileRole::Child ? "CHILD" : "PARENT",
                       x + card_width / 2, 342, 2, kMuted);
  }

  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B BACK", kWidth / 2, 438, 2,
                     kMuted);
}

void render_home(SDL_Renderer* renderer, const LauncherState& state) {
  const auto* profile = state.active_profile();
  if (profile == nullptr) {
    return;
  }

  draw_text(renderer, "HELLO, " + profile->display_name, 54, 36, 4, kText);
  draw_text(renderer, state.screen() == Screen::ChildHome ? "CHILD MODE" : "PARENT MODE",
            56, 78, 2, color_from_rgb(profile->accent_rgb));

  const auto items = state.menu_items();
  const int row_start = items.size() > 8 ? 98 : (items.size() > 7 ? 104 : (items.size() > 6 ? 108 : 118));
  const int row_gap = items.size() > 8 ? 34 : (items.size() > 7 ? 38 : (items.size() > 6 ? 43 : 49));
  const int row_height = items.size() > 8 ? 27 : (items.size() > 7 ? 31 : (items.size() > 6 ? 35 : 39));
  for (std::size_t index = 0; index < items.size(); ++index) {
    const SDL_Rect row{88, row_start + static_cast<int>(index) * row_gap, 464,
                       row_height};
    fill_rect(renderer, row, index == state.focus_index() ? kPanelFocused : kPanel);
    if (index == state.focus_index()) {
      outline_rect(renderer, row, 3, kFocus);
      draw_text(renderer, ">", 104, row.y + (row_height - 14) / 2, 2, kFocus);
    }
    draw_text(renderer, items[index], 132, row.y + (row_height - 14) / 2, 2, kText);
  }

  draw_centered_text(renderer, "PREVIEW DATA ONLY", kWidth / 2, 424, 2, kMuted);
  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B PROFILES", kWidth / 2, 452, 2,
                     kMuted);
}

int setup_step_number(SetupStep step) {
  return step == SetupStep::Complete ? 11 : static_cast<int>(step) + 1;
}

}  // namespace

bool render_startup_splash(SDL_Renderer* renderer,
                           const std::filesystem::path& image_path) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  SDL_Texture* texture =
      IMG_LoadTexture(renderer, path_as_utf8(image_path).c_str());
  if (texture == nullptr) {
    return false;
  }
  const SDL_Rect canvas{0, 0, kWidth, kHeight};
  SDL_RenderCopy(renderer, texture, nullptr, &canvas);
  SDL_DestroyTexture(texture);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  fill_rect(renderer, {78, 34, 484, 86}, {18, 45, 34, 198});
  draw_centered_text(renderer, "SPROUTOS", kWidth / 2, 51, 7, kText);
  SDL_RenderPresent(renderer);
  return true;
}

void render_launcher(SDL_Renderer* renderer, const LauncherState& state,
                     const std::filesystem::path& managed_image_root,
                     const std::filesystem::path& background_image,
                     const std::filesystem::path& accent_atlas,
                     const std::filesystem::path& built_in_avatar_root) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  if (!background_image.empty()) {
    SDL_Texture* texture =
        IMG_LoadTexture(renderer, path_as_utf8(background_image).c_str());
    if (texture != nullptr) {
      const SDL_Rect canvas{0, 0, kWidth, kHeight};
      SDL_RenderCopy(renderer, texture, nullptr, &canvas);
      SDL_DestroyTexture(texture);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      fill_rect(renderer, canvas, {8, 26, 20, 128});
    }
  }

  if (!accent_atlas.empty()) {
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
    render_home(renderer, state);
  }

  SDL_RenderPresent(renderer);
}

void render_library(SDL_Renderer* renderer,
                    const LibraryPresentation& library) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);

  draw_text(renderer, library.title(), 48, 32, 4, kText);
  draw_text(renderer, library.source_label(), 50, 76, 2, kMuted);

  const auto entries = library.entries();
  if (entries.empty()) {
    const SDL_Rect panel{70, 150, 500, 180};
    fill_rect(renderer, panel, kPanel);
    draw_centered_text(renderer, library.empty_message(), kWidth / 2, 218, 2,
                       kText);
    draw_centered_text(renderer, "ADD SUPPORTED GAMES OR GO BACK", kWidth / 2,
                       260, 1, kMuted);
  } else {
    constexpr std::size_t visible_count = 6;
    const std::size_t first = library.focus_index() < visible_count
                                  ? 0
                                  : library.focus_index() - visible_count + 1;
    const std::size_t last = std::min(entries.size(), first + visible_count);
    for (std::size_t index = first; index < last; ++index) {
      const auto& entry = entries[index];
      const int row_number = static_cast<int>(index - first);
      const SDL_Rect row{64, 112 + row_number * 48, 512, 38};
      const bool focused = index == library.focus_index();
      fill_rect(renderer, row, focused ? kPanelFocused : kPanel);
      if (focused) {
        outline_rect(renderer, row, 3, kFocus);
        draw_text(renderer, ">", 78, row.y + 12, 2, kFocus);
      }
      const std::string title = entry.title.size() > 30
                                    ? entry.title.substr(0, 30)
                                    : entry.title;
      draw_text(renderer, title, 106, row.y + 12, 2,
                entry.launch_allowed && entry.unavailable_reason.empty()
                    ? kText
                    : kMuted);
      draw_text(renderer, entry.platform_label, 476,
                row.y + 12, 2, kMuted);
      if (entry.favorite) {
        draw_text(renderer, "*", 548, row.y + 12, 2, kFocus);
      }
    }
  }

  if (!library.notice().empty()) {
    draw_centered_text(renderer, library.notice().substr(0, 68), kWidth / 2,
                       410, 1, kFocus);
  }
  draw_centered_text(renderer, "ARROWS MOVE   A PLAY   B BACK", kWidth / 2, 450,
                     1, kMuted);
  SDL_RenderPresent(renderer);
}

void render_profile_archive(SDL_Renderer* renderer,
                            const ProfileArchivePresentation& archive) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);

  draw_centered_text(renderer, archive.title(), kWidth / 2, 34, 3, kText);
  draw_centered_text(renderer, archive.description(), kWidth / 2, 76, 1,
                     kMuted);

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
  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B BACK", kWidth / 2,
                     450, 1, kMuted);
  SDL_RenderPresent(renderer);
}

void render_profile_avatars(
    SDL_Renderer* renderer, const ProfileAvatarPresentation& presentation,
    const std::filesystem::path& built_in_avatar_root) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);

  draw_centered_text(renderer, "PROFILE IMAGES", kWidth / 2, 24, 4, kText);
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
  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B BACK", kWidth / 2,
                     452, 1, kMuted);
  SDL_RenderPresent(renderer);
}

void render_profile_image_crop(SDL_Renderer* renderer,
                               const ProfileImageCropPresentation& crop) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  draw_centered_text(renderer, "CROP PROFILE PORTRAIT", kWidth / 2, 34, 3, kText);
  draw_centered_text(renderer, "MOVE THE PHOTO INSIDE THE SQUARE", kWidth / 2, 76, 1,
                     kMuted);

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
  draw_centered_text(renderer, "ZOOM " + std::to_string(zoom_percent) + "%",
                     kWidth / 2, 386, 2, kText);
  if (!crop.error_message().empty()) {
    draw_centered_text(renderer, crop.error_message().substr(0, 68), kWidth / 2, 414, 1,
                       kFocus);
  }
  draw_centered_text(renderer, "ARROWS MOVE   L R ZOOM   A USE   B CANCEL",
                     kWidth / 2, 450, 1, kMuted);
  SDL_RenderPresent(renderer);
}

void render_parent_pin(SDL_Renderer* renderer,
                       const ParentPinPresentation& pin) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);
  draw_centered_text(renderer, pin.title(), kWidth / 2, 28, 3, kText);
  draw_centered_text(renderer, pin.description(), kWidth / 2, 64, 1, kMuted);

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
  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B CANCEL", kWidth / 2, 450, 1,
                     kMuted);
  SDL_RenderPresent(renderer);
}

void render_setup(SDL_Renderer* renderer, const SetupPresentation& setup) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);

  draw_centered_text(renderer, "SPROUT", kWidth / 2, 30, 4, kText);
  const std::string progress = "STEP " + std::to_string(setup_step_number(setup.step())) +
                               " OF 11";
  draw_centered_text(renderer, progress, kWidth / 2, 72, 2, kMuted);

  const SDL_Rect panel{54, 112, 532, 276};
  fill_rect(renderer, panel, kPanel);
  outline_rect(renderer, panel, 2, kPanelFocused);
  draw_centered_text(renderer, setup.title(), kWidth / 2, 144, 3, kText);
  draw_centered_text(renderer, setup.description(), kWidth / 2, 192, 1, kMuted);

  const auto choices = setup.choices();
  for (std::size_t index = 0; index < choices.size(); ++index) {
    const SDL_Rect choice{124, 246 + static_cast<int>(index) * 58, 392, 44};
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
  draw_centered_text(renderer, "ARROWS CHOOSE   A CONTINUE   B SAVE AND EXIT",
                     kWidth / 2, 444, 1, kMuted);
  SDL_RenderPresent(renderer);
}

void render_recovery(SDL_Renderer* renderer,
                     const RecoveryPresentation& recovery) {
  set_color(renderer, kBackground);
  SDL_RenderClear(renderer);

  draw_centered_text(renderer, recovery.title(), kWidth / 2, 34, 3, kText);
  draw_centered_text(renderer, recovery.description(), kWidth / 2, 76, 1,
                     kMuted);

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
  draw_centered_text(renderer, "ARROWS MOVE   A SELECT   B BACK", kWidth / 2,
                     450, 1, kMuted);
  SDL_RenderPresent(renderer);
}

}  // namespace sprout::launcher
