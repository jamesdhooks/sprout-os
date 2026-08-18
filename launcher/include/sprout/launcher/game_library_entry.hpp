#pragma once

#include "sprout/launcher/native_launch_adapter.hpp"
#include "sprout/launcher/onion_launch_adapter.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace sprout::launcher {

using LibraryLaunchTarget =
    std::variant<EmulatedLaunchTarget, NativeLaunchTarget>;

// Inventory and launch metadata shared by discovery and the unified dashboard.
// This deliberately contains no presentation state.
struct LibraryEntry {
  std::string id;
  std::string title;
  std::string platform_label;
  std::filesystem::path artwork_path;
  LibraryLaunchTarget launch_target;
  bool child_visible{false};
  bool favorite{false};
  std::optional<std::size_t> recent_rank;
  bool launch_allowed{true};
  std::string unavailable_reason;
};

}  // namespace sprout::launcher
