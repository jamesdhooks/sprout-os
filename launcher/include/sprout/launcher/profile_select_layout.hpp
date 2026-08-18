#pragma once

#include <cstddef>
#include <vector>

namespace sprout::launcher {

struct ProfileSelectSlot {
  int row{0};
  int center_x{0};
  int center_y{0};
  int avatar_size{0};
  int visual_left{0};
  int visual_right{0};
  int visual_top{0};
  int name_bottom{0};
};

// Computes a centered, two-row-at-most profile layout for the Miyoo's 640x480
// viewport. Visual bounds include focus outline, rotation, and bobbing safety.
[[nodiscard]] std::size_t profile_select_column_count(
    std::size_t profile_count) noexcept;

[[nodiscard]] std::vector<ProfileSelectSlot> profile_select_layout(
    int profile_count, int focused_index);

}  // namespace sprout::launcher
