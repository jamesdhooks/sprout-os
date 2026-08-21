#include "sprout/launcher/profile_select_layout.hpp"

#include <stdexcept>

namespace sprout::launcher {

std::vector<ProfileSelectSlot> profile_select_layout(int profile_count,
                                                      int focused_index) {
  if (profile_count < 0 || profile_count > 8) {
    throw std::invalid_argument("Profile count must be in [0, 8]");
  }
  if (profile_count == 0) return {};
  if (focused_index < 0 || focused_index >= profile_count) {
    throw std::invalid_argument("Focused profile index is out of range");
  }

  std::vector<ProfileSelectSlot> slots;
  slots.reserve(static_cast<std::size_t>(profile_count));
  for (int index = 0; index < profile_count; ++index) {
    const bool focused = index == focused_index;
    int offset = index - focused_index;
    if (offset > profile_count / 2) offset -= profile_count;
    if (offset < -(profile_count / 2)) offset += profile_count;
    const int center_x = 320 + offset * 230;
    const int center_y = 232;
    const int avatar_size = focused ? 232 : 168;
    const int visual_extent = avatar_size / 2 + (focused ? 8 : 5);
    const int name_height = focused ? 38 : 34;
    const int name_bottom =
        center_y + avatar_size / 2 + 12 + name_height;
    slots.push_back({center_x, center_y, avatar_size,
                     center_x - visual_extent, center_x + visual_extent,
                     center_y - visual_extent, name_bottom});
  }
  return slots;
}

}  // namespace sprout::launcher
