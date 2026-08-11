#pragma once

#include "sprout/launcher/read_only_view.hpp"

#include <cstdint>
#include <vector>

namespace sprout::launcher {

struct PaddedPortraitAlpha {
  std::vector<std::uint8_t> alpha;
  int width{0};
  int height{0};
  int inset{0};
};

// Adds enough transparent canvas for the requested outline radius. This keeps
// artwork that touches an image edge from clipping its generated outer ring.
[[nodiscard]] PaddedPortraitAlpha pad_portrait_alpha(
    ReadOnlyView<std::uint8_t> source_alpha, int width, int height,
    float radius);

// Builds only the feathered pixels outside a portrait's existing alpha. The
// caller chooses the border color at render time, so artwork remains theme-neutral.
[[nodiscard]] std::vector<std::uint8_t> smooth_portrait_outline(
    ReadOnlyView<std::uint8_t> source_alpha, int width, int height,
    float radius);

}  // namespace sprout::launcher
