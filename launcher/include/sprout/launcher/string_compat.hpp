#pragma once

#include <string_view>

namespace sprout::launcher {

[[nodiscard]] constexpr bool starts_with(std::string_view value,
                                         std::string_view prefix) noexcept {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace sprout::launcher
