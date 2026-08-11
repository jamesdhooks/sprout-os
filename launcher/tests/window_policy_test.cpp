#include "sprout/launcher/window_policy.hpp"

#include <cassert>

int main() {
  using sprout::launcher::requires_fullscreen_window;

  assert(requires_fullscreen_window("mmiyoo"));
  assert(requires_fullscreen_window("MMIYOO"));
  assert(!requires_fullscreen_window("windows"));
  assert(!requires_fullscreen_window("x11"));
  assert(!requires_fullscreen_window(""));
}