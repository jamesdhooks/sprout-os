#pragma once

#include <cstddef>
#include <vector>

namespace sprout::launcher {

struct ProfileSelectSlot {
  int center_x{0};
  int center_y{0};
  int avatar_size{0};
  int visual_left{0};
  int visual_right{0};
  int visual_top{0};
  int name_bottom{0};
};

// Computes the horizontally looping profile carousel. The focused portrait is
// always centered; neighbouring portraits deliberately extend toward the
// viewport edges so the next direction is obvious.
[[nodiscard]] std::vector<ProfileSelectSlot> profile_select_layout(
    int profile_count, int focused_index);

}  // namespace sprout::launcher
