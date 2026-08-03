#include "sprout/launcher/portrait_outline.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

int main() {
  std::vector<std::uint8_t> alpha(49, 0);
  alpha[3 * 7 + 3] = 255;
  const auto outline =
      sprout::launcher::smooth_portrait_outline(alpha, 7, 7, 2.0F);

  assert(outline[3 * 7 + 3] == 0);
  assert(outline[3 * 7 + 4] == 255);
  assert(outline[3 * 7 + 5] > 0);
  assert(outline[3 * 7 + 6] == 0);
  assert(outline[0] == 0);

  bool rejected = false;
  try {
    static_cast<void>(
        sprout::launcher::smooth_portrait_outline(alpha, 8, 7, 2.0F));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}
