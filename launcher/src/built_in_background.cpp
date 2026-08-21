#include "sprout/launcher/built_in_background.hpp"

#include <array>
#include <string>

namespace sprout::launcher {
namespace {

constexpr std::array<BuiltInBackground, 20> kBackgrounds{{
    {"garden-morning", "Garden Morning", "garden-morning.png", 0, false},
    {"firefly-evening", "Firefly Evening", "firefly-evening.png", 0, false},
    {"sunny-cove", "Sunny Cove", "sunny-cove.png", 0, false},
    {"treehouse-library", "Treehouse Library", "treehouse-library.png", 0, false},
    {"color-sky", "Sky", "", 0x8DD8F8, true},
    {"color-ocean", "Ocean", "", 0x2476B8, true},
    {"color-mint", "Mint", "", 0x8EE0B0, true},
    {"color-forest", "Forest", "", 0x2E7D52, true},
    {"color-sun", "Sun", "", 0xFFD44A, true},
    {"color-orange", "Orange", "", 0xF58B39, true},
    {"color-berry", "Berry", "", 0xD9537E, true},
    {"color-lavender", "Lavender", "", 0xA889D6, true},
    {"color-night", "Night", "", 0x293553, true},
    {"color-slate", "Slate", "", 0x617184, true},
    {"color-cloud", "Cloud", "", 0xDCE7EF, true},
    {"color-sand", "Sand", "", 0xE8C999, true},
    {"color-rose", "Rose", "", 0xF1A7B7, true},
    {"color-plum", "Plum", "", 0x754A84, true},
    {"color-teal", "Teal", "", 0x24989A, true},
    {"color-cocoa", "Cocoa", "", 0x795548, true},
}};

}  // namespace

ReadOnlyView<BuiltInBackground> built_in_backgrounds() noexcept {
  return {kBackgrounds.data(), kBackgrounds.size()};
}

const BuiltInBackground* find_built_in_background(std::string_view id) noexcept {
  for (const auto& background : kBackgrounds) {
    if (background.id == id) return &background;
  }
  return nullptr;
}

std::string built_in_background_ref(std::string_view id) {
  return "builtin:" + std::string(id);
}

}  // namespace sprout::launcher
