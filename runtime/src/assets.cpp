#include "sprout/runtime/assets.hpp"

#include <yyjson.h>

#include <array>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace sprout::runtime {
namespace {

constexpr std::uintmax_t kMaximumManifestBytes = 512U * 1024U;
constexpr std::size_t kMaximumAtlases = 16;
constexpr std::size_t kMaximumSprites = 2048;
constexpr std::size_t kMaximumAnimations = 512;
constexpr std::size_t kMaximumAnimationFrames = 128;
constexpr std::size_t kMaximumTileSets = 128;

void validate_keys(yyjson_val* object,
                   std::initializer_list<std::string_view> allowed,
                   std::string_view context) {
  if (!yyjson_is_obj(object)) {
    throw std::runtime_error(std::string(context) + " should be an object");
  }
  const std::set<std::string_view> allowed_keys(allowed);
  std::set<std::string> observed;
  yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string name = yyjson_get_str(key);
    if (allowed_keys.find(name) == allowed_keys.end() ||
        !observed.insert(name).second) {
      throw std::runtime_error(std::string(context) +
                               " contains an unknown or duplicate field: " + name);
    }
  }
}

yyjson_val* required(yyjson_val* object, const char* key,
                     std::string_view context) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (value == nullptr) {
    throw std::runtime_error(std::string(context) + " is missing field: " + key);
  }
  return value;
}

std::string text(yyjson_val* object, const char* key,
                 std::string_view context) {
  yyjson_val* value = required(object, key, context);
  if (!yyjson_is_str(value) || yyjson_get_len(value) == 0 ||
      yyjson_get_len(value) > 128) {
    throw std::runtime_error(std::string(context) + " field should be text: " + key);
  }
  return {yyjson_get_str(value), yyjson_get_len(value)};
}

std::pair<int, int> pair_of_uints(yyjson_val* object, const char* key,
                                  std::string_view context, int maximum) {
  yyjson_val* value = required(object, key, context);
  if (!yyjson_is_arr(value) || yyjson_arr_size(value) != 2) {
    throw std::runtime_error(std::string(context) + " field should contain two values: " + key);
  }
  yyjson_val* first = yyjson_arr_get_first(value);
  yyjson_val* second = yyjson_arr_get(value, 1);
  if (!yyjson_is_uint(first) || !yyjson_is_uint(second) ||
      yyjson_get_uint(first) > static_cast<std::uint64_t>(maximum) ||
      yyjson_get_uint(second) > static_cast<std::uint64_t>(maximum)) {
    throw std::runtime_error(std::string(context) + " field is outside bounds: " + key);
  }
  return {static_cast<int>(yyjson_get_uint(first)),
          static_cast<int>(yyjson_get_uint(second))};
}

std::array<int, 4> rectangle(yyjson_val* object, const char* key,
                             std::string_view context) {
  yyjson_val* value = required(object, key, context);
  if (!yyjson_is_arr(value) || yyjson_arr_size(value) != 4) {
    throw std::runtime_error(std::string(context) + " rect should contain four values");
  }
  std::array<int, 4> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    yyjson_val* part = yyjson_arr_get(value, index);
    if (!yyjson_is_uint(part) || yyjson_get_uint(part) > 4096) {
      throw std::runtime_error(std::string(context) + " rect is outside bounds");
    }
    result[index] = static_cast<int>(yyjson_get_uint(part));
  }
  if (result[2] == 0 || result[3] == 0) {
    throw std::runtime_error(std::string(context) + " rect dimensions should be positive");
  }
  return result;
}

std::string read_file(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > kMaximumManifestBytes) {
    throw std::runtime_error("Asset manifest is unavailable or too large");
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("Could not open asset manifest");
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

bool is_within(const std::filesystem::path& root,
               const std::filesystem::path& candidate) {
  auto root_part = root.begin();
  auto candidate_part = candidate.begin();
  while (root_part != root.end() && candidate_part != candidate.end()) {
    if (*root_part != *candidate_part) return false;
    ++root_part;
    ++candidate_part;
  }
  return root_part == root.end();
}

template <typename Map>
std::size_t lookup(const Map& values, const std::string& id,
                   std::string_view context) {
  const auto found = values.find(id);
  if (found == values.end()) {
    throw std::runtime_error(std::string(context) + " references unknown id: " + id);
  }
  return found->second;
}

}  // namespace

AssetCatalogue load_assets(const std::filesystem::path& package_root,
                           const std::filesystem::path& manifest_path) {
  const std::string encoded = read_file(manifest_path);
  yyjson_read_err error{};
  yyjson_doc* document = yyjson_read_opts(const_cast<char*>(encoded.data()),
                                           encoded.size(), YYJSON_READ_NOFLAG,
                                           nullptr, &error);
  if (document == nullptr) {
    throw std::runtime_error(std::string("Invalid asset manifest JSON: ") + error.msg);
  }

  try {
    AssetCatalogue result;
    yyjson_val* root = yyjson_doc_get_root(document);
    validate_keys(root, {"schemaVersion", "atlases", "sprites", "animations", "tileSets", "provenance"},
                  "Asset manifest");
    yyjson_val* version = required(root, "schemaVersion", "Asset manifest");
    if (!yyjson_is_uint(version) || yyjson_get_uint(version) != 1) {
      throw std::runtime_error("Unsupported asset manifest schema version");
    }
    yyjson_val* provenance = required(root, "provenance", "Asset manifest");
    validate_keys(provenance, {"source", "license", "generator", "palette"},
                  "Asset provenance");
    static_cast<void>(text(provenance, "source", "Asset provenance"));
    static_cast<void>(text(provenance, "license", "Asset provenance"));
    static_cast<void>(text(provenance, "generator", "Asset provenance"));
    static_cast<void>(text(provenance, "palette", "Asset provenance"));

    std::unordered_map<std::string, std::size_t> atlas_ids;
    yyjson_val* atlases = required(root, "atlases", "Asset manifest");
    if (!yyjson_is_arr(atlases) || yyjson_arr_size(atlases) == 0 ||
        yyjson_arr_size(atlases) > kMaximumAtlases) {
      throw std::runtime_error("Asset manifest atlas count is outside bounds");
    }
    std::size_t index = 0, maximum = 0;
    yyjson_val* value = nullptr;
    yyjson_arr_foreach(atlases, index, maximum, value) {
      validate_keys(value, {"id", "image", "size"}, "Atlas");
      TextureAtlas atlas;
      atlas.id = text(value, "id", "Atlas");
      if (!atlas_ids.emplace(atlas.id, result.atlases.size()).second) {
        throw std::runtime_error("Duplicate atlas id: " + atlas.id);
      }
      const std::filesystem::path relative = text(value, "image", "Atlas");
      if (relative.is_absolute() || relative.extension() != ".png") {
        throw std::runtime_error("Atlas image should be a relative PNG path");
      }
      std::error_code path_error;
      atlas.image = std::filesystem::canonical(package_root / relative, path_error);
      if (path_error || !std::filesystem::is_regular_file(atlas.image) ||
          !is_within(package_root, atlas.image)) {
        throw std::runtime_error("Atlas image escapes or is missing from package root");
      }
      const auto size = pair_of_uints(value, "size", "Atlas", 4096);
      if (size.first == 0 || size.second == 0) {
        throw std::runtime_error("Atlas dimensions should be positive");
      }
      atlas.width = size.first;
      atlas.height = size.second;
      result.atlases.push_back(std::move(atlas));
    }

    yyjson_val* sprites = required(root, "sprites", "Asset manifest");
    if (!yyjson_is_arr(sprites) || yyjson_arr_size(sprites) == 0 ||
        yyjson_arr_size(sprites) > kMaximumSprites) {
      throw std::runtime_error("Asset manifest sprite count is outside bounds");
    }
    yyjson_arr_foreach(sprites, index, maximum, value) {
      validate_keys(value, {"id", "atlas", "rect", "pivot"}, "Sprite");
      SpriteFrame sprite;
      sprite.id = text(value, "id", "Sprite");
      if (!result.sprite_ids.emplace(sprite.id, result.sprites.size()).second) {
        throw std::runtime_error("Duplicate sprite id: " + sprite.id);
      }
      sprite.atlas = lookup(atlas_ids, text(value, "atlas", "Sprite"), "Sprite");
      const auto bounds = rectangle(value, "rect", "Sprite");
      sprite.x = bounds[0]; sprite.y = bounds[1];
      sprite.width = bounds[2]; sprite.height = bounds[3];
      if (yyjson_obj_get(value, "pivot") != nullptr) {
        const auto pivot = pair_of_uints(value, "pivot", "Sprite", 4096);
        sprite.pivot_x = pivot.first; sprite.pivot_y = pivot.second;
      }
      const auto& atlas = result.atlases[sprite.atlas];
      if (sprite.x + sprite.width > atlas.width ||
          sprite.y + sprite.height > atlas.height ||
          sprite.pivot_x > sprite.width || sprite.pivot_y > sprite.height) {
        throw std::runtime_error("Sprite lies outside its atlas or has invalid pivot: " + sprite.id);
      }
      result.sprites.push_back(std::move(sprite));
    }

    yyjson_val* animations = required(root, "animations", "Asset manifest");
    if (!yyjson_is_arr(animations) || yyjson_arr_size(animations) > kMaximumAnimations) {
      throw std::runtime_error("Asset manifest animation count is outside bounds");
    }
    yyjson_arr_foreach(animations, index, maximum, value) {
      validate_keys(value, {"id", "loop", "frames"}, "Animation");
      SpriteAnimation animation;
      animation.id = text(value, "id", "Animation");
      if (!result.animation_ids.emplace(animation.id, result.animations.size()).second) {
        throw std::runtime_error("Duplicate animation id: " + animation.id);
      }
      yyjson_val* loop = required(value, "loop", "Animation");
      if (!yyjson_is_bool(loop)) throw std::runtime_error("Animation loop should be boolean");
      animation.loop = yyjson_get_bool(loop);
      yyjson_val* frames = required(value, "frames", "Animation");
      if (!yyjson_is_arr(frames) || yyjson_arr_size(frames) == 0 ||
          yyjson_arr_size(frames) > kMaximumAnimationFrames) {
        throw std::runtime_error("Animation frame count is outside bounds");
      }
      std::size_t frame_index = 0, frame_maximum = 0;
      yyjson_val* frame = nullptr;
      yyjson_arr_foreach(frames, frame_index, frame_maximum, frame) {
        validate_keys(frame, {"sprite", "ticks"}, "Animation frame");
        yyjson_val* ticks = required(frame, "ticks", "Animation frame");
        if (!yyjson_is_uint(ticks) || yyjson_get_uint(ticks) == 0 ||
            yyjson_get_uint(ticks) > 3600 ||
            animation.total_ticks + yyjson_get_uint(ticks) > 65535) {
          throw std::runtime_error("Animation duration is outside bounds");
        }
        animation.frames.push_back({
            lookup(result.sprite_ids, text(frame, "sprite", "Animation frame"), "Animation"),
            static_cast<std::uint16_t>(yyjson_get_uint(ticks))});
        animation.total_ticks += static_cast<std::uint32_t>(yyjson_get_uint(ticks));
      }
      result.animations.push_back(std::move(animation));
    }

    yyjson_val* tile_sets = required(root, "tileSets", "Asset manifest");
    if (!yyjson_is_arr(tile_sets) || yyjson_arr_size(tile_sets) > kMaximumTileSets) {
      throw std::runtime_error("Asset manifest tile-set count is outside bounds");
    }
    yyjson_arr_foreach(tile_sets, index, maximum, value) {
      validate_keys(value, {"id", "tileSize", "sprites"}, "Tile set");
      TileSet tile_set;
      tile_set.id = text(value, "id", "Tile set");
      if (!result.tile_set_ids.emplace(tile_set.id, result.tile_sets.size()).second) {
        throw std::runtime_error("Duplicate tile-set id: " + tile_set.id);
      }
      const auto size = pair_of_uints(value, "tileSize", "Tile set", 256);
      if (size.first == 0 || size.second == 0) {
        throw std::runtime_error("Tile size should be positive");
      }
      tile_set.tile_width = size.first; tile_set.tile_height = size.second;
      yyjson_val* entries = required(value, "sprites", "Tile set");
      if (!yyjson_is_arr(entries) || yyjson_arr_size(entries) == 0 ||
          yyjson_arr_size(entries) > 256) {
        throw std::runtime_error("Tile-set sprite count is outside bounds");
      }
      std::size_t entry_index = 0, entry_maximum = 0;
      yyjson_val* entry = nullptr;
      yyjson_arr_foreach(entries, entry_index, entry_maximum, entry) {
        if (!yyjson_is_str(entry)) throw std::runtime_error("Tile-set sprite id should be text");
        const std::string id(yyjson_get_str(entry), yyjson_get_len(entry));
        const std::size_t sprite_index = lookup(result.sprite_ids, id, "Tile set");
        tile_set.sprites.push_back(sprite_index);
      }
      result.tile_sets.push_back(std::move(tile_set));
    }

    yyjson_doc_free(document);
    return result;
  } catch (...) {
    yyjson_doc_free(document);
    throw;
  }
}

}  // namespace sprout::runtime
