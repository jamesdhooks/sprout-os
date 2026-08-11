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

  // Artwork may touch its source edges. Padding must create transparent canvas
  // before dilation so the external focus ring cannot be texture-clipped.
  const std::vector<std::uint8_t> edge_alpha{255};
  const auto padded =
      sprout::launcher::pad_portrait_alpha(edge_alpha, 1, 1, 2.0F);
  assert(padded.inset == 3);
  assert(padded.width == 7);
  assert(padded.height == 7);
  assert(padded.alpha[3 * 7 + 3] == 255);
  const auto padded_outline = sprout::launcher::smooth_portrait_outline(
      padded.alpha, padded.width, padded.height, 2.0F);
  assert(padded_outline[3 * 7 + 1] > 0);
  assert(padded_outline[3 * 7 + 5] > 0);
  assert(padded_outline[1 * 7 + 3] > 0);
  assert(padded_outline[5 * 7 + 3] > 0);
}
