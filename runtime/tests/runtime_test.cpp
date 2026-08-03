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

function capture_scenario(name)
  if name ~= "review" then error("unknown capture scenario") end
  score = 314
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
    check(drawing.size() == 1 &&
              drawing.front().type == sprout::runtime::DrawCommandType::Rectangle &&
              drawing.front().rectangle.x == 10,
          "render command was not captured");
    auto changed = first.drain_events();
    check(changed.size() == 1 && changed.front().type == "AchievementUnlocked",
          "score event was not emitted");
    first.apply_capture_scenario("review");
    check(first.snapshot() == "314",
          "capture scenario did not establish deterministic state");
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
    check(snake_package.title_screen.enabled &&
              snake_package.title_screen.image_width == 640 &&
              snake_package.title_screen.image_height == 480,
          "Snake title presentation was not loaded");
    check(snake_package.assets.atlases.size() == 2 &&
              snake_package.assets.sprites.size() == 32 &&
              snake_package.assets.animations.size() == 4,
          "Snake rich asset catalogue was not loaded");
    sprout::runtime::Session snake_first(snake_package, root / "snake-first", 7);
    sprout::runtime::Session snake_second(snake_package, root / "snake-second", 7);
    snake_first.start();
    snake_second.start();
    snake_first.apply_capture_scenario("gameplay");
    snake_second.apply_capture_scenario("gameplay");
    for (int tick = 0; tick < 80; ++tick) {
      const sprout::runtime::Actions actions{.down = tick == 18,
                                             .left = tick == 42};
      snake_first.step(actions);
      snake_second.step(actions);
    }
    check(snake_first.snapshot() == snake_second.snapshot(),
          "Snake was not deterministic for identical input");
    check(!snake_first.render().empty(), "Snake did not render any content");
    snake_first.apply_capture_scenario("fail");
    check(snake_first.snapshot().starts_with("ended:7:7:"),
          "Snake fail capture scenario was not applied");

    const auto mouse_path =
        std::filesystem::path(SPROUT_SOURCE_DIR) / "games" / "mouse-maze";
    const auto mouse_package = sprout::runtime::load_package(mouse_path);
    check(mouse_package.title_screen.enabled,
          "Mouse Maze title presentation was not loaded");
    check(mouse_package.assets.atlases.size() == 3 &&
              mouse_package.assets.sprites.size() == 44 &&
              mouse_package.assets.animations.size() == 10 &&
              mouse_package.assets.tile_sets.size() == 1,
          "Mouse Maze asset catalogue was not loaded");
    sprout::runtime::Session mouse(mouse_package, root / "mouse", 7);
    mouse.start();
    mouse.apply_capture_scenario("gameplay");
    const auto initial_mouse_drawing = mouse.render();
    check(initial_mouse_drawing.size() > 160 &&
              initial_mouse_drawing.back().type ==
                  sprout::runtime::DrawCommandType::Sprite,
          "Mouse Maze did not submit its tilemap and animated sprite");
    const int initial_mouse_frame = initial_mouse_drawing.back().sprite.source_x;
    check(initial_mouse_drawing.back().sprite.width <
              initial_mouse_drawing.back().sprite.source_width,
          "fractional sprite scaling did not preserve the rich source frame");
    for (int tick = 0; tick < 8; ++tick) mouse.step({});
    check(mouse.render().back().sprite.source_x != initial_mouse_frame,
          "engine animation did not advance from the fixed session tick");
    mouse.apply_capture_scenario("win");
    check(mouse.snapshot() == "13:9:right:1",
          "Mouse Maze win capture scenario was not applied");

    const auto blocks_path = std::filesystem::path(SPROUT_SOURCE_DIR) / "games" /
                             "blocks-buttons";
    const auto blocks_package = sprout::runtime::load_package(blocks_path);
    check(blocks_package.title_screen.enabled,
          "Blocks & Buttons title presentation was not loaded");
    check(blocks_package.assets.atlases.size() == 3 &&
              blocks_package.assets.sprites.size() == 47 &&
              blocks_package.assets.animations.size() == 9,
          "Blocks & Buttons asset catalogue was not loaded");
    sprout::runtime::Session blocks(blocks_package, root / "blocks", 7);
    blocks.start();
    blocks.apply_capture_scenario("gameplay");
    const auto& blocks_drawing = blocks.render();
    check(blocks_drawing.size() > 80 &&
              blocks_drawing.back().type ==
                  sprout::runtime::DrawCommandType::Sprite,
          "Blocks & Buttons did not submit its sprite scene");
    blocks.apply_capture_scenario("win");
    check(blocks.snapshot() == "6:5:right:1:0:7:2:7:5",
          "Blocks & Buttons win capture scenario was not applied");
    blocks.apply_capture_scenario("fail");
    check(blocks.snapshot() == "2:2:left:0:1:1:1:5:4",
          "Blocks & Buttons fail capture scenario was not applied");
    blocks.apply_capture_scenario("deadlock-test");
    blocks.step({.left = true});
    check(blocks.snapshot() == "2:1:left:0:1:1:1:5:4",
          "Blocks & Buttons did not detect a provable corner deadlock");

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
