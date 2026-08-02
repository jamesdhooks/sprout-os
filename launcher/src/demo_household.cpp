#include "sprout/launcher/launcher_state.hpp"

namespace sprout::launcher {

std::vector<Profile> make_demo_household() {
  return {
      Profile{
          .id = "child-alex",
          .display_name = "Alex",
          .role = ProfileRole::Child,
          .accent_rgb = 0x70B77E,
          .avatar_ref = "builtin:sprout",
      },
      Profile{
          .id = "parent-preview",
          .display_name = "Parent",
          .role = ProfileRole::Parent,
          .accent_rgb = 0x8E7DBE,
          .avatar_ref = "builtin:fox",
      },
  };
}

}  // namespace sprout::launcher
