#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sprout::launcher {

// Builds only the feathered pixels outside a portrait's existing alpha. The
// caller chooses the border color at render time, so artwork remains theme-neutral.
[[nodiscard]] std::vector<std::uint8_t> smooth_portrait_outline(
    std::span<const std::uint8_t> source_alpha, int width, int height,
    float radius);

}  // namespace sprout::launcher
