#pragma once

#include "sprout/runtime/package.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
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
};

enum class DrawCommandType {
  Rectangle,
  Circle,
  Sprite,
  Label,
};

struct DrawCommand {
  DrawCommandType type{DrawCommandType::Rectangle};
  DrawRect rectangle;
  DrawCircle circle;
  DrawSprite sprite;
  DrawLabel label;
};

struct RuntimeEvent {
  std::string type;
  std::string value;
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
  void apply_capture_scenario(std::string_view scenario);
  void stop();
  const std::vector<DrawCommand>& render();
  std::vector<RuntimeEvent> drain_events();
  std::string snapshot() const;

  const PackageManifest& package() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::runtime
