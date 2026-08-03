#pragma once

#include "sprout/runtime/package.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
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
  void stop();
  const std::vector<DrawRect>& render();
  std::vector<RuntimeEvent> drain_events();
  std::string snapshot() const;

  const PackageManifest& package() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::runtime
