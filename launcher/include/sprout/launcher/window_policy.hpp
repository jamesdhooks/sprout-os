#pragma once

#include <string_view>

namespace sprout::launcher {

[[nodiscard]] bool requires_fullscreen_window(std::string_view video_driver);

}  // namespace sprout::launcher
