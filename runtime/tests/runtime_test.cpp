#include "sprout/runtime/package.hpp"
#include "sprout/runtime/session.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void write(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << contents;
  if (!stream) {
    throw std::runtime_error("Could not create runtime test fixture");
  }
}

std::filesystem::path create_package(const std::filesystem::path& root) {
  const auto package = root / "package";
  write(package / "manifest.json", R"({
  "schemaVersion": 1,
  "id": "sprout.runtime-test",
  "title": "Runtime Test",
  "version": "1.0.0",
  "runtimeVersion": 1,
  "entrypoint": "game.lua",
  "logicalResolution": [320, 240],
  "audience": "family",
  "capabilities": ["events", "local-storage"]
})");
  write(package / "game.lua", R"(
local score = 0

function init()
  score = sprout.storage_get("score", 0)
end

function update(actions)
  if actions.primary then
    score = score + sprout.random(100)
    sprout.storage_set("score", score)
    sprout.emit("AchievementUnlocked", tostring(score))
  end
end

function render()
  sprout.rect(10, 20, 30, 40, 80, 160, 90)
end

function snapshot()
  return tostring(score)
end
)");
  return package;
}

}  // namespace

int main() {
  const auto unique = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root = std::filesystem::temp_directory_path() /
                    ("sprout-runtime-test-" + unique);
  try {
    const auto package_path = create_package(root);
    const auto package = sprout::runtime::load_package(package_path);
    check(package.id == "sprout.runtime-test", "package id was not loaded");
    check(package.logical_width == 320 && package.logical_height == 240,
          "logical resolution was not loaded");

    sprout::runtime::Session first(package, root / "first-storage", 42);
    first.start();
    auto started = first.drain_events();
    check(started.size() == 1 && started.front().type == "GameStarted",
          "start event was not emitted");
    first.step({.primary = true});
    const std::string first_snapshot = first.snapshot();
    const auto& drawing = first.render();
    check(drawing.size() == 1 && drawing.front().x == 10,
          "render command was not captured");
    auto changed = first.drain_events();
    check(changed.size() == 1 && changed.front().type == "AchievementUnlocked",
          "score event was not emitted");
    first.stop();
    auto stopped = first.drain_events();
    check(stopped.size() == 1 && stopped.front().type == "GameExited",
          "exit event was not emitted by the runtime");

    sprout::runtime::Session repeated(package, root / "repeat-storage", 42);
    repeated.start();
    repeated.drain_events();
    repeated.step({.primary = true});
    check(repeated.snapshot() == first_snapshot,
          "identical seeds and input were not deterministic");

    sprout::runtime::Session restored(package, root / "first-storage", 999);
    restored.start();
    check(restored.snapshot() == first_snapshot,
          "local game storage was not restored");

    const auto snake_path =
        std::filesystem::path(SPROUT_SOURCE_DIR) / "games" / "snake";
    const auto snake_package = sprout::runtime::load_package(snake_path);
    sprout::runtime::Session snake_first(snake_package, root / "snake-first", 7);
    sprout::runtime::Session snake_second(snake_package, root / "snake-second", 7);
    snake_first.start();
    snake_second.start();
    for (int tick = 0; tick < 80; ++tick) {
      const sprout::runtime::Actions actions{.down = tick == 18,
                                             .left = tick == 42};
      snake_first.step(actions);
      snake_second.step(actions);
    }
    check(snake_first.snapshot() == snake_second.snapshot(),
          "Snake was not deterministic for identical input");
    check(!snake_first.render().empty(), "Snake did not render any content");

    write(package_path / "game.lua", R"(
function init()
  while true do end
end
function update(actions) end
function render() end
function snapshot() return "" end
)");
    bool limited = false;
    try {
      sprout::runtime::Session runaway(package, root / "runaway-storage", 1);
      runaway.start();
    } catch (const std::runtime_error& error) {
      limited = std::string(error.what()).find("instruction limit") !=
                std::string::npos;
    }
    check(limited, "runaway lifecycle call was not stopped");

    write(package_path / "manifest.json", R"({"schemaVersion":1,"unexpected":true})");
    bool rejected = false;
    try {
      static_cast<void>(sprout::runtime::load_package(package_path));
    } catch (const std::runtime_error&) {
      rejected = true;
    }
    check(rejected, "invalid package manifest was accepted");

    std::filesystem::remove_all(root);
    std::cout << "runtime tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    return 1;
  }
}
