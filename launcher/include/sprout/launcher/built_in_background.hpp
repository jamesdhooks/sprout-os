#pragma once

#include "sprout/launcher/read_only_view.hpp"

#include <string_view>
#include <cstdint>

namespace sprout::launcher {

struct BuiltInBackground {
  std::string_view id;
  std::string_view display_name;
  std::string_view filename;
  std::uint32_t color_rgb{0};
  bool flat_color{false};
};

[[nodiscard]] ReadOnlyView<BuiltInBackground> built_in_backgrounds() noexcept;
[[nodiscard]] const BuiltInBackground* find_built_in_background(
    std::string_view id) noexcept;
[[nodiscard]] std::string built_in_background_ref(std::string_view id);

}  // namespace sprout::launcher
