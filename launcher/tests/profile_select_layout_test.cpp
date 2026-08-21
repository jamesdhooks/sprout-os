#include "sprout/launcher/profile_select_layout.hpp"

#include <cassert>
#include <cstddef>

int main() {
  using sprout::launcher::profile_select_layout;

  for (int count = 1; count <= 8; ++count) {
    for (int focused = 0; focused < count; ++focused) {
      const auto slots = profile_select_layout(count, focused);
      assert(static_cast<int>(slots.size()) == count);
      const auto& selected = slots[static_cast<std::size_t>(focused)];
      assert(selected.center_x == 320);
      assert(selected.center_y == 232);
      assert(selected.avatar_size == 232);
      assert(selected.visual_top >= 96);
      assert(selected.name_bottom <= 410);
    }
  }
}
