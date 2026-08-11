#include "sprout/launcher/profile_select_layout.hpp"

#include <cassert>
#include <cstddef>

int main() {
  using sprout::launcher::profile_select_layout;

  for (int count = 1; count <= 8; ++count) {
    for (int focused = 0; focused < count; ++focused) {
      const auto slots = profile_select_layout(count, focused);
      assert(static_cast<int>(slots.size()) == count);
      for (const auto& slot : slots) {
        assert(slot.visual_left >= 12);
        assert(slot.visual_right <= 628);
        assert(slot.visual_top >= 72);
        assert(slot.name_bottom <= 432);
      }
      for (std::size_t left = 0; left < slots.size(); ++left) {
        for (std::size_t right = left + 1; right < slots.size(); ++right) {
          if (slots[left].row != slots[right].row) continue;
          assert(slots[right].visual_left - slots[left].visual_right >= 36);
        }
      }
    }
  }
}
