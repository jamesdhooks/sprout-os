#include "sprout/launcher/built_in_avatar.hpp"

#include <array>
#include <stdexcept>

namespace sprout::launcher {
namespace {

constexpr std::array<BuiltInAvatar, 32> kAvatars{{
    {"rocket-ship", "ROCKET SHIP"},
    {"sunflower", "SUNFLOWER"},
    {"unicorn", "UNICORN"},
    {"red-panda", "RED PANDA"},
    {"friendly-dragon", "FRIENDLY DRAGON"},
    {"hot-air-balloon", "HOT-AIR BALLOON"},
    {"sea-turtle", "SEA TURTLE"},
    {"robot", "ROBOT"},
    {"bumblebee", "BUMBLEBEE"},
    {"storybook-castle", "STORYBOOK CASTLE"},
    {"comet", "COMET"},
    {"axolotl", "AXOLOTL"},
    {"pirate-ship", "PIRATE SHIP"},
    {"mushroom-house", "MUSHROOM HOUSE"},
    {"penguin", "PENGUIN"},
    {"butterfly", "BUTTERFLY"},
    {"triceratops", "TRICERATOPS"},
    {"moon-rabbit", "MOON RABBIT"},
    {"fire-truck", "FIRE TRUCK"},
    {"cupcake", "CUPCAKE"},
    {"octopus", "OCTOPUS"},
    {"explorer-fox", "EXPLORER FOX"},
    {"koala", "KOALA"},
    {"magical-book", "MAGICAL BOOK"},
    {"snow-leopard", "SNOW LEOPARD"},
    {"ladybug", "LADYBUG"},
    {"submarine", "SUBMARINE"},
    {"happy-cactus", "HAPPY CACTUS"},
    {"phoenix", "PHOENIX"},
    {"astronaut-cat", "ASTRONAUT CAT"},
    {"crystal-geode", "CRYSTAL GEODE"},
    {"smiling-planet", "SMILING PLANET"},
}};

bool safe_id(std::string_view id) noexcept {
  if (id.empty()) return false;
  for (const char value : id) {
    if ((value < 'a' || value > 'z') && (value < '0' || value > '9') &&
        value != '-') {
      return false;
    }
  }
  return true;
}

}  // namespace

ReadOnlyView<BuiltInAvatar> built_in_avatars() noexcept { return kAvatars; }

const BuiltInAvatar* find_built_in_avatar(
    std::string_view avatar_ref) noexcept {
  constexpr std::string_view prefix = "builtin:";
  const auto id = avatar_ref.starts_with(prefix)
                      ? avatar_ref.substr(prefix.size())
                      : avatar_ref;
  for (const auto& avatar : kAvatars) {
    if (avatar.id == id) return &avatar;
  }
  return nullptr;
}

std::string built_in_avatar_ref(std::string_view id) {
  if (!safe_id(id) || find_built_in_avatar(id) == nullptr) {
    throw std::invalid_argument("Built-in avatar id is not in the catalogue");
  }
  return "builtin:" + std::string(id);
}

std::filesystem::path built_in_avatar_master_path(
    const std::filesystem::path& avatar_root, std::string_view id) {
  if (!safe_id(id) || find_built_in_avatar(id) == nullptr) {
    throw std::invalid_argument("Built-in avatar id is not in the catalogue");
  }
  return avatar_root / "masters" / (std::string(id) + ".png");
}

std::filesystem::path built_in_avatar_thumbnail_path(
    const std::filesystem::path& avatar_root, std::string_view id) {
  if (!safe_id(id) || find_built_in_avatar(id) == nullptr) {
    throw std::invalid_argument("Built-in avatar id is not in the catalogue");
  }
  return avatar_root / "thumbs" / (std::string(id) + ".png");
}

}  // namespace sprout::launcher
