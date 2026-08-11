#include "sprout/launcher/profile_select_layout.hpp"

#include <algorithm>
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

  const int columns = std::max(1, std::min(4, (profile_count + 1) / 2));
  const int rows = (profile_count + columns - 1) / columns;
  const int spacing_x = columns == 1 ? 0 : columns == 2 ? 220
                                      : columns == 3   ? 188
                                                       : 160;
  const int row_spacing = rows == 1 ? 0 : 174;
  const int base_y = rows == 1 ? 212 : 142;

  std::vector<ProfileSelectSlot> slots;
  slots.reserve(static_cast<std::size_t>(profile_count));
  for (int index = 0; index < profile_count; ++index) {
    const bool focused = index == focused_index;
    const int row = index / columns;
    const int column = index % columns;
    const int items_in_row = std::min(columns, profile_count - row * columns);
    const int row_width = (items_in_row - 1) * spacing_x;
    const int center_x = 320 - row_width / 2 + column * spacing_x;
    const int center_y = base_y + row * row_spacing;
    const int avatar_size = focused ? 116 : 96;
    const int visual_extent = focused ? 64 : 52;
    const int name_height = focused ? 38 : 34;
    const int name_bottom =
        center_y + avatar_size / 2 + 12 + name_height;
    slots.push_back({row, center_x, center_y, avatar_size,
                     center_x - visual_extent, center_x + visual_extent,
                     center_y - visual_extent, name_bottom});
  }
  return slots;
}

}  // namespace sprout::launcher
