#include "sprout/launcher/window_policy.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace sprout::launcher {

bool requires_fullscreen_window(std::string_view video_driver) {
  std::string normalized(video_driver);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return normalized == "mmiyoo";
}

}  // namespace sprout::launcher
