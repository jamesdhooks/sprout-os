#pragma once

#include "sprout/runtime/package.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::runtime {

struct Actions {
  bool up{};
  bool down{};
  bool left{};
  bool right{};
  bool primary{};
  bool secondary{};
  bool start{};
  bool back{};
};

struct DrawRect {
  int x{};
  int y{};
  int width{};
  int height{};
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{255};
};

struct DrawSprite {
  std::size_t atlas{};
  int source_x{};
  int source_y{};
  int source_width{};
  int source_height{};
  int x{};
  int y{};
  int width{};
  int height{};
  bool flip_x{};
  bool flip_y{};
  std::uint8_t red{255};
  std::uint8_t green{255};
  std::uint8_t blue{255};
  std::uint8_t alpha{255};
};

struct DrawCircle {
  int x{};
  int y{};
  int radius{};
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{255};
};

struct DrawRoundedRect {
  DrawRect rectangle;
  int radius{};
};

struct DrawLabel {
  std::string text;
  int x{};
  int y{};
  int width{};
  int height{};
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{255};
  bool display{};
};

enum class DrawCommandType {
  Rectangle,
  RoundedRectangle,
  Circle,
  Sprite,
  Label,
};

struct DrawCommand {
  DrawCommandType type{DrawCommandType::Rectangle};
  DrawRect rectangle;
  DrawRoundedRect rounded_rectangle;
  DrawCircle circle;
  DrawSprite sprite;
  DrawLabel label;
};

struct RuntimeEvent {
  std::string type;
  std::string value;
};

struct SoundRequest {
  std::size_t sound{};
  std::uint8_t volume{255};
};

class Session {
 public:
  Session(PackageManifest package, std::filesystem::path storage_root,
          std::uint64_t seed);
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) noexcept;
  Session& operator=(Session&&) noexcept;

  void start();
  void step(const Actions& actions);
  void step_title(const Actions& actions);
  void apply_capture_scenario(std::string_view scenario);
  void stop();
  const std::vector<DrawCommand>& render();
  std::vector<RuntimeEvent> drain_events();
  std::vector<SoundRequest> drain_sounds();
  std::string snapshot() const;
  std::optional<std::string> title_status() const;
  bool can_reset_progress() const;
  bool reset_progress();

  const PackageManifest& package() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::runtime
