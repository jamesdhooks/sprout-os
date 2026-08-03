#include "sprout/launcher/built_in_background.hpp"

#include <array>
#include <string>

namespace sprout::launcher {
namespace {

constexpr std::array<BuiltInBackground, 4> kBackgrounds{{
    {"garden-morning", "Garden Morning", "garden-morning.png"},
    {"firefly-evening", "Firefly Evening", "firefly-evening.png"},
    {"sunny-cove", "Sunny Cove", "sunny-cove.png"},
    {"treehouse-library", "Treehouse Library", "treehouse-library.png"},
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
