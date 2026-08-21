#pragma once

#include "sprout/launcher/game_library_entry.hpp"

#include <vector>

namespace sprout::launcher {

[[nodiscard]] std::vector<LibraryEntry> make_demo_library();

}  // namespace sprout::launcher
