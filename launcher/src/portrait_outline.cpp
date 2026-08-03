#include "sprout/launcher/portrait_outline.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sprout::launcher {

std::vector<std::uint8_t> smooth_portrait_outline(
    ReadOnlyView<std::uint8_t> source_alpha, int width, int height,
    float radius) {
  if (width <= 0 || height <= 0 ||
      source_alpha.size() != static_cast<std::size_t>(width * height)) {
    throw std::invalid_argument("Portrait alpha dimensions are invalid");
  }
  if (!std::isfinite(radius) || radius <= 0.0F || radius > 32.0F) {
    throw std::invalid_argument("Portrait outline radius must be in (0, 32]");
  }

  std::vector<std::uint8_t> outline(source_alpha.size(), 0);
  const int reach = static_cast<int>(std::ceil(radius + 0.5F));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y * width + x);
      const float existing = static_cast<float>(source_alpha[index]);
      if (existing >= 255.0F) continue;

      float expanded = 0.0F;
      for (int offset_y = -reach; offset_y <= reach; ++offset_y) {
        const int source_y = y + offset_y;
        if (source_y < 0 || source_y >= height) continue;
        for (int offset_x = -reach; offset_x <= reach; ++offset_x) {
          const int source_x = x + offset_x;
          if (source_x < 0 || source_x >= width) continue;
          const float distance = std::sqrt(static_cast<float>(
              offset_x * offset_x + offset_y * offset_y));
          const float edge_coverage =
              std::clamp(radius + 0.5F - distance, 0.0F, 1.0F);
          if (edge_coverage <= 0.0F) continue;
          const auto neighbor = static_cast<std::size_t>(source_y * width + source_x);
          expanded = std::max(
              expanded,
              static_cast<float>(source_alpha[neighbor]) * edge_coverage);
        }
      }
      outline[index] = static_cast<std::uint8_t>(
          std::clamp(expanded - existing, 0.0F, 255.0F));
    }
  }
  return outline;
}

}  // namespace sprout::launcher
