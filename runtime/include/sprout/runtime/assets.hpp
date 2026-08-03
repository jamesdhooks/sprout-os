#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace sprout::runtime {

struct TextureAtlas {
  std::string id;
  std::filesystem::path image;
  int width{};
  int height{};
};

struct SpriteFrame {
  std::string id;
  std::size_t atlas{};
  int x{};
  int y{};
  int width{};
  int height{};
  int pivot_x{};
  int pivot_y{};
};

struct AnimationFrame {
  std::size_t sprite{};
  std::uint16_t ticks{};
};

struct SpriteAnimation {
  std::string id;
  std::vector<AnimationFrame> frames;
  std::uint32_t total_ticks{};
  bool loop{true};
};

struct TileSet {
  std::string id;
  int tile_width{};
  int tile_height{};
  std::vector<std::size_t> sprites;
};

struct AssetCatalogue {
  std::vector<TextureAtlas> atlases;
  std::vector<SpriteFrame> sprites;
  std::vector<SpriteAnimation> animations;
  std::vector<TileSet> tile_sets;
  std::unordered_map<std::string, std::size_t> sprite_ids;
  std::unordered_map<std::string, std::size_t> animation_ids;
  std::unordered_map<std::string, std::size_t> tile_set_ids;

  bool empty() const noexcept { return atlases.empty(); }
};

AssetCatalogue load_assets(const std::filesystem::path& package_root,
                           const std::filesystem::path& manifest_path);

}  // namespace sprout::runtime
