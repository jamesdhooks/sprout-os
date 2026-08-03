#include "sprout/runtime/session.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <yyjson.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace sprout::runtime {
namespace {

constexpr std::size_t kMaximumDrawCommands = 4096;

bool has_capability(const PackageManifest& package, std::string_view capability) {
  return std::find(package.capabilities.begin(), package.capabilities.end(),
                   capability) != package.capabilities.end();
}

bool valid_storage_key(std::string_view key) {
  return !key.empty() && key.size() <= 64 &&
         std::all_of(key.begin(), key.end(), [](unsigned char character) {
           return std::isalnum(character) != 0 || character == '.' ||
                  character == '-' || character == '_';
         });
}

bool valid_event_type(std::string_view type) {
  return type == "AchievementUnlocked" || type == "LevelCompleted";
}

void replace_file(const std::filesystem::path& pending,
                  const std::filesystem::path& destination) {
#ifdef _WIN32
  if (!MoveFileExW(pending.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error("Could not activate native-game storage");
  }
#else
  if (std::rename(pending.c_str(), destination.c_str()) != 0) {
    throw std::runtime_error("Could not activate native-game storage");
  }
#endif
}

}  // namespace

struct Session::Impl {
  PackageManifest package;
  std::filesystem::path storage_path;
  std::uint64_t random_state;
  lua_State* lua{};
  bool started{};
  bool stopped{};
  std::uint64_t tick{};
  std::map<std::string, std::int64_t> storage;
  std::vector<DrawCommand> drawing;
  std::vector<RuntimeEvent> events;

  Impl(PackageManifest loaded_package, std::filesystem::path storage_root,
       std::uint64_t seed)
      : package(std::move(loaded_package)),
        storage_path(std::move(storage_root) / package.id / "storage.json"),
        random_state(seed == 0 ? 0x9e3779b97f4a7c15ULL : seed),
        lua(luaL_newstate()) {
    if (lua == nullptr) {
      throw std::runtime_error("Could not allocate native-game interpreter");
    }
    load_storage();
  }

  ~Impl() {
    if (lua != nullptr) {
      lua_close(lua);
    }
  }

  static Impl& self(lua_State* state) {
    return *static_cast<Impl*>(lua_touserdata(state, lua_upvalueindex(1)));
  }

  static int random(lua_State* state) {
    auto& runtime = self(state);
    const auto maximum = luaL_checkinteger(state, 1);
    if (maximum <= 0) {
      return luaL_error(state, "random maximum should be positive");
    }
    std::uint64_t value = runtime.random_state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    runtime.random_state = value;
    value *= 2685821657736338717ULL;
    lua_pushinteger(state,
                    static_cast<lua_Integer>(value %
                                             static_cast<std::uint64_t>(maximum)) +
                        1);
    return 1;
  }

  static int rect(lua_State* state) {
    auto& runtime = self(state);
    DrawRect rectangle{
        .x = static_cast<int>(luaL_checkinteger(state, 1)),
        .y = static_cast<int>(luaL_checkinteger(state, 2)),
        .width = static_cast<int>(luaL_checkinteger(state, 3)),
        .height = static_cast<int>(luaL_checkinteger(state, 4)),
        .red = static_cast<std::uint8_t>(luaL_checkinteger(state, 5)),
        .green = static_cast<std::uint8_t>(luaL_checkinteger(state, 6)),
        .blue = static_cast<std::uint8_t>(luaL_checkinteger(state, 7)),
        .alpha = static_cast<std::uint8_t>(luaL_optinteger(state, 8, 255)),
    };
    const auto valid_color = [state](int index) {
      const auto component = lua_tointeger(state, index);
      return component >= 0 && component <= 255;
    };
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width <= 0 ||
        rectangle.height <= 0 ||
        rectangle.x + rectangle.width > runtime.package.logical_width ||
        rectangle.y + rectangle.height > runtime.package.logical_height ||
        !valid_color(5) || !valid_color(6) || !valid_color(7) ||
        (lua_gettop(state) >= 8 && !valid_color(8))) {
      return luaL_error(state, "rectangle is outside the logical surface");
    }
    if (runtime.drawing.size() >= kMaximumDrawCommands) {
      return luaL_error(state, "frame draw-command limit exceeded");
    }
    runtime.drawing.push_back(
        {.type = DrawCommandType::Rectangle, .rectangle = rectangle});
    return 0;
  }

  static std::string identifier(lua_State* state, int argument) {
    std::size_t length = 0;
    const char* value = luaL_checklstring(state, argument, &length);
    if (length == 0 || length > 128) {
      luaL_argerror(state, argument, "asset id is outside bounds");
    }
    return {value, length};
  }

  static void append_sprite(lua_State* state, Impl& runtime,
                            std::size_t sprite_index, int x, int y, double scale,
                            bool flip_x, bool flip_y, int alpha) {
    if (sprite_index >= runtime.package.assets.sprites.size()) {
      luaL_error(state, "sprite index is outside bounds");
      return;
    }
    if (!std::isfinite(scale) || scale < (1.0 / 64.0) || scale > 64.0 ||
        alpha < 0 || alpha > 255) {
      luaL_error(state, "sprite scale or alpha is outside bounds");
      return;
    }
    const auto& frame = runtime.package.assets.sprites[sprite_index];
    const int target_x = x - static_cast<int>(std::lround(frame.pivot_x * scale));
    const int target_y = y - static_cast<int>(std::lround(frame.pivot_y * scale));
    const int width = (std::max)(1, static_cast<int>(std::lround(frame.width * scale)));
    const int height = (std::max)(1, static_cast<int>(std::lround(frame.height * scale)));
    if (target_x < -width || target_y < -height ||
        target_x >= runtime.package.logical_width ||
        target_y >= runtime.package.logical_height) {
      luaL_error(state, "sprite is outside the logical surface");
      return;
    }
    if (runtime.drawing.size() >= kMaximumDrawCommands) {
      luaL_error(state, "frame draw-command limit exceeded");
      return;
    }
    runtime.drawing.push_back({
        .type = DrawCommandType::Sprite,
        .sprite = {.atlas = frame.atlas,
                   .source_x = frame.x,
                   .source_y = frame.y,
                   .source_width = frame.width,
                   .source_height = frame.height,
                   .x = target_x,
                   .y = target_y,
                   .width = width,
                   .height = height,
                   .flip_x = flip_x,
                   .flip_y = flip_y,
                   .alpha = static_cast<std::uint8_t>(alpha)}});
  }

  static int sprite(lua_State* state) {
    auto& runtime = self(state);
    const std::string id = identifier(state, 1);
    const auto found = runtime.package.assets.sprite_ids.find(id);
    if (found == runtime.package.assets.sprite_ids.end()) {
      return luaL_error(state, "unknown sprite id: %s", id.c_str());
    }
    append_sprite(state, runtime, found->second,
                  static_cast<int>(luaL_checkinteger(state, 2)),
                  static_cast<int>(luaL_checkinteger(state, 3)),
                  static_cast<double>(luaL_optnumber(state, 4, 1.0)),
                  lua_toboolean(state, 5) != 0,
                  lua_toboolean(state, 6) != 0,
                  static_cast<int>(luaL_optinteger(state, 7, 255)));
    return 0;
  }

  static int animate(lua_State* state) {
    auto& runtime = self(state);
    const std::string id = identifier(state, 1);
    const auto found = runtime.package.assets.animation_ids.find(id);
    if (found == runtime.package.assets.animation_ids.end()) {
      return luaL_error(state, "unknown animation id: %s", id.c_str());
    }
    const auto& animation = runtime.package.assets.animations[found->second];
    const lua_Integer phase_value = luaL_optinteger(state, 4, 0);
    if (phase_value < 0) return luaL_error(state, "animation phase should not be negative");
    std::uint64_t position = runtime.tick + static_cast<std::uint64_t>(phase_value);
    if (animation.loop) {
      position %= animation.total_ticks;
    } else if (position >= animation.total_ticks) {
      position = animation.total_ticks - 1;
    }
    std::size_t sprite_index = animation.frames.back().sprite;
    for (const auto& frame : animation.frames) {
      if (position < frame.ticks) {
        sprite_index = frame.sprite;
        break;
      }
      position -= frame.ticks;
    }
    append_sprite(state, runtime, sprite_index,
                  static_cast<int>(luaL_checkinteger(state, 2)),
                  static_cast<int>(luaL_checkinteger(state, 3)),
                  static_cast<double>(luaL_optnumber(state, 5, 1.0)),
                  lua_toboolean(state, 6) != 0,
                  lua_toboolean(state, 7) != 0,
                  static_cast<int>(luaL_optinteger(state, 8, 255)));
    return 0;
  }

  static lua_Integer table_integer(lua_State* state, int table_index,
                                   const char* field, lua_Integer fallback,
                                   bool required_value = false) {
    lua_getfield(state, table_index, field);
    lua_Integer result = fallback;
    if (lua_isnil(state, -1)) {
      if (required_value) {
        lua_pop(state, 1);
        luaL_error(state, "sprite batch item is missing field: %s", field);
      }
    } else if (!lua_isinteger(state, -1)) {
      lua_pop(state, 1);
      luaL_error(state, "sprite batch field should be an integer: %s", field);
    } else {
      result = lua_tointeger(state, -1);
    }
    lua_pop(state, 1);
    return result;
  }

  static lua_Number table_number(lua_State* state, int table_index,
                                 const char* field, lua_Number fallback) {
    lua_getfield(state, table_index, field);
    lua_Number result = fallback;
    if (!lua_isnil(state, -1)) {
      if (!lua_isnumber(state, -1)) {
        lua_pop(state, 1);
        luaL_error(state, "sprite batch field should be numeric: %s", field);
      }
      result = lua_tonumber(state, -1);
    }
    lua_pop(state, 1);
    return result;
  }

  static bool table_boolean(lua_State* state, int table_index,
                            const char* field) {
    lua_getfield(state, table_index, field);
    if (!lua_isnil(state, -1) && !lua_isboolean(state, -1)) {
      lua_pop(state, 1);
      luaL_error(state, "sprite batch field should be boolean: %s", field);
    }
    const bool result = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return result;
  }

  static std::string table_identifier(lua_State* state, int table_index,
                                      const char* field) {
    lua_getfield(state, table_index, field);
    std::string result;
    if (!lua_isnil(state, -1)) {
      std::size_t length = 0;
      const char* value = luaL_checklstring(state, -1, &length);
      if (length == 0 || length > 128) {
        lua_pop(state, 1);
        luaL_error(state, "sprite batch asset id is outside bounds");
      }
      result.assign(value, length);
    }
    lua_pop(state, 1);
    return result;
  }

  static int sprite_batch(lua_State* state) {
    auto& runtime = self(state);
    luaL_checktype(state, 1, LUA_TTABLE);
    const std::size_t count = lua_rawlen(state, 1);
    if (count == 0 || count > kMaximumDrawCommands ||
        runtime.drawing.size() + count > kMaximumDrawCommands) {
      return luaL_error(state, "sprite batch size is outside bounds");
    }
    for (std::size_t index = 1; index <= count; ++index) {
      lua_geti(state, 1, static_cast<lua_Integer>(index));
      if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return luaL_error(state, "sprite batch item should be a table");
      }
      const int item = lua_gettop(state);
      const std::string sprite_id = table_identifier(state, item, "sprite");
      const std::string animation_id = table_identifier(state, item, "animation");
      if (sprite_id.empty() == animation_id.empty()) {
        lua_pop(state, 1);
        return luaL_error(state,
                          "sprite batch item should name one sprite or animation");
      }
      std::size_t sprite_index = 0;
      if (!sprite_id.empty()) {
        const auto found = runtime.package.assets.sprite_ids.find(sprite_id);
        if (found == runtime.package.assets.sprite_ids.end()) {
          lua_pop(state, 1);
          return luaL_error(state, "unknown sprite id: %s", sprite_id.c_str());
        }
        sprite_index = found->second;
      } else {
        const auto found = runtime.package.assets.animation_ids.find(animation_id);
        if (found == runtime.package.assets.animation_ids.end()) {
          lua_pop(state, 1);
          return luaL_error(state, "unknown animation id: %s",
                            animation_id.c_str());
        }
        const auto& animation = runtime.package.assets.animations[found->second];
        const lua_Integer phase = table_integer(state, item, "phase", 0);
        if (phase < 0) {
          lua_pop(state, 1);
          return luaL_error(state, "animation phase should not be negative");
        }
        std::uint64_t position = runtime.tick + static_cast<std::uint64_t>(phase);
        position = animation.loop
                       ? position % animation.total_ticks
                       : std::min<std::uint64_t>(position,
                                                 animation.total_ticks - 1);
        sprite_index = animation.frames.back().sprite;
        for (const auto& frame : animation.frames) {
          if (position < frame.ticks) {
            sprite_index = frame.sprite;
            break;
          }
          position -= frame.ticks;
        }
      }
      append_sprite(
          state, runtime, sprite_index,
          static_cast<int>(table_integer(state, item, "x", 0, true)),
          static_cast<int>(table_integer(state, item, "y", 0, true)),
          static_cast<double>(table_number(state, item, "scale", 1.0)),
          table_boolean(state, item, "flipX"),
          table_boolean(state, item, "flipY"),
          static_cast<int>(table_integer(state, item, "alpha", 255)));
      lua_pop(state, 1);
    }
    return 0;
  }

  static int tilemap(lua_State* state) {
    auto& runtime = self(state);
    const std::string id = identifier(state, 1);
    const auto found = runtime.package.assets.tile_set_ids.find(id);
    if (found == runtime.package.assets.tile_set_ids.end()) {
      return luaL_error(state, "unknown tile-set id: %s", id.c_str());
    }
    std::size_t length = 0;
    const auto* tiles = reinterpret_cast<const unsigned char*>(
        luaL_checklstring(state, 2, &length));
    const int columns = static_cast<int>(luaL_checkinteger(state, 3));
    const int origin_x = static_cast<int>(luaL_checkinteger(state, 4));
    const int origin_y = static_cast<int>(luaL_checkinteger(state, 5));
    const int scale = static_cast<int>(luaL_optinteger(state, 6, 1));
    if (length == 0 || length > kMaximumDrawCommands || columns <= 0 ||
        columns > 256 || length % static_cast<std::size_t>(columns) != 0 ||
        scale < 1 || scale > 8) {
      return luaL_error(state, "tilemap dimensions are outside bounds");
    }
    const auto& tile_set = runtime.package.assets.tile_sets[found->second];
    for (std::size_t tile = 0; tile < length; ++tile) {
      if (tiles[tile] >= tile_set.sprites.size()) {
        return luaL_error(state, "tilemap contains an unknown tile index");
      }
      const int column = static_cast<int>(tile % static_cast<std::size_t>(columns));
      const int row = static_cast<int>(tile / static_cast<std::size_t>(columns));
      append_sprite(state, runtime, tile_set.sprites[tiles[tile]],
                    origin_x + column * tile_set.tile_width * scale,
                    origin_y + row * tile_set.tile_height * scale,
                    scale, false, false, 255);
    }
    return 0;
  }

  static int emit(lua_State* state) {
    auto& runtime = self(state);
    if (!has_capability(runtime.package, "events")) {
      return luaL_error(state, "package did not declare the events capability");
    }
    std::size_t type_length = 0;
    std::size_t value_length = 0;
    const char* type_text = luaL_checklstring(state, 1, &type_length);
    const char* value_text = luaL_optlstring(state, 2, "", &value_length);
    const std::string type(type_text, type_length);
    const std::string value(value_text, value_length);
    if (!valid_event_type(type) || value.size() > 256) {
      return luaL_error(state, "event type or value is invalid");
    }
    runtime.events.push_back({type, value});
    return 0;
  }

  static int storage_get(lua_State* state) {
    auto& runtime = self(state);
    if (!has_capability(runtime.package, "local-storage")) {
      return luaL_error(state,
                        "package did not declare the local-storage capability");
    }
    std::size_t key_length = 0;
    const char* key_text = luaL_checklstring(state, 1, &key_length);
    const std::string key(key_text, key_length);
    const auto fallback = luaL_optinteger(state, 2, 0);
    if (!valid_storage_key(key)) {
      return luaL_error(state, "storage key is invalid");
    }
    const auto found = runtime.storage.find(key);
    lua_pushinteger(state, found == runtime.storage.end()
                               ? fallback
                               : static_cast<lua_Integer>(found->second));
    return 1;
  }

  static int storage_set(lua_State* state) {
    auto& runtime = self(state);
    if (!has_capability(runtime.package, "local-storage")) {
      return luaL_error(state,
                        "package did not declare the local-storage capability");
    }
    std::size_t key_length = 0;
    const char* key_text = luaL_checklstring(state, 1, &key_length);
    const std::string key(key_text, key_length);
    const auto value = luaL_checkinteger(state, 2);
    if (!valid_storage_key(key)) {
      return luaL_error(state, "storage key is invalid");
    }
    runtime.storage[key] = static_cast<std::int64_t>(value);
    try {
      runtime.save_storage();
    } catch (const std::exception& error) {
      return luaL_error(state, "%s", error.what());
    }
    return 0;
  }

  void open_library(const char* name, lua_CFunction function) {
    luaL_requiref(lua, name, function, 1);
    lua_pop(lua, 1);
  }

  void install_api() {
    open_library(LUA_GNAME, luaopen_base);
    open_library(LUA_TABLIBNAME, luaopen_table);
    open_library(LUA_STRLIBNAME, luaopen_string);
    open_library(LUA_MATHLIBNAME, luaopen_math);
    open_library(LUA_UTF8LIBNAME, luaopen_utf8);

    for (const char* name : {"dofile", "load", "loadfile", "collectgarbage"}) {
      lua_pushnil(lua);
      lua_setglobal(lua, name);
    }
    lua_getglobal(lua, LUA_MATHLIBNAME);
    lua_pushnil(lua);
    lua_setfield(lua, -2, "random");
    lua_pushnil(lua);
    lua_setfield(lua, -2, "randomseed");
    lua_pop(lua, 1);

    lua_newtable(lua);
    const auto add = [this](const char* name, lua_CFunction function) {
      lua_pushlightuserdata(lua, this);
      lua_pushcclosure(lua, function, 1);
      lua_setfield(lua, -2, name);
    };
    add("random", random);
    add("rect", rect);
    add("sprite", sprite);
    add("animate", animate);
    add("sprite_batch", sprite_batch);
    add("tilemap", tilemap);
    add("emit", emit);
    add("storage_get", storage_get);
    add("storage_set", storage_set);
    lua_setglobal(lua, "sprout");
  }

  void call(const char* name, int arguments, int results = 0) const {
    const int function_index = lua_gettop(lua) - arguments;
    lua_getglobal(lua, name);
    if (!lua_isfunction(lua, -1)) {
      lua_pop(lua, 1);
      lua_settop(lua, function_index);
      throw std::runtime_error(std::string("Game is missing lifecycle function: ") +
                               name);
    }
    lua_insert(lua, function_index + 1);
    lua_sethook(lua, instruction_limit, LUA_MASKCOUNT, 100000);
    const int result = lua_pcall(lua, arguments, results, 0);
    lua_sethook(lua, nullptr, 0, 0);
    if (result != LUA_OK) {
      const std::string message = lua_tostring(lua, -1);
      lua_pop(lua, 1);
      throw std::runtime_error(std::string("Game ") + name + " failed: " + message);
    }
  }

  static void instruction_limit(lua_State* state, lua_Debug*) {
    luaL_error(state, "lifecycle instruction limit exceeded");
  }

  void load_storage() {
    std::error_code error;
    if (!std::filesystem::exists(storage_path, error)) {
      return;
    }
    std::ifstream stream(storage_path, std::ios::binary);
    if (!stream) {
      throw std::runtime_error("Could not open native-game storage");
    }
    const std::string encoded{std::istreambuf_iterator<char>(stream),
                              std::istreambuf_iterator<char>()};
    if (encoded.size() > 64U * 1024U) {
      throw std::runtime_error("Native-game storage is too large");
    }
    yyjson_doc* document = yyjson_read(encoded.data(), encoded.size(), 0);
    if (document == nullptr) {
      throw std::runtime_error("Native-game storage is invalid JSON");
    }
    yyjson_val* root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
      yyjson_doc_free(document);
      throw std::runtime_error("Native-game storage should be an object");
    }
    yyjson_obj_iter iterator = yyjson_obj_iter_with(root);
    while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
      yyjson_val* value = yyjson_obj_iter_get_val(key);
      const std::string name(yyjson_get_str(key), yyjson_get_len(key));
      if (!valid_storage_key(name) || !yyjson_is_int(value)) {
        yyjson_doc_free(document);
        throw std::runtime_error("Native-game storage contains an invalid entry");
      }
      storage.emplace(name, yyjson_get_sint(value));
    }
    yyjson_doc_free(document);
  }

  void save_storage() const {
    std::filesystem::create_directories(storage_path.parent_path());
    yyjson_mut_doc* document = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* root = yyjson_mut_obj(document);
    yyjson_mut_doc_set_root(document, root);
    for (const auto& [key, value] : storage) {
      yyjson_mut_obj_add_val(document, root, key.c_str(),
                             yyjson_mut_sint(document, value));
    }
    std::size_t length = 0;
    char* encoded = yyjson_mut_write(document, YYJSON_WRITE_PRETTY, &length);
    yyjson_mut_doc_free(document);
    if (encoded == nullptr) {
      throw std::runtime_error("Could not encode native-game storage");
    }
    const auto pending = storage_path.string() + ".pending";
    std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
    stream.write(encoded, static_cast<std::streamsize>(length));
    stream.put('\n');
    std::free(encoded);
    stream.flush();
    if (!stream) {
      throw std::runtime_error("Could not write native-game storage");
    }
    stream.close();
    replace_file(pending, storage_path);
  }
};

Session::Session(PackageManifest package, std::filesystem::path storage_root,
                 std::uint64_t seed)
    : impl_(std::make_unique<Impl>(std::move(package), std::move(storage_root),
                                   seed)) {}

Session::~Session() = default;
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

void Session::start() {
  if (impl_->started) {
    throw std::runtime_error("Native-game session is already started");
  }
  impl_->install_api();
  if (luaL_loadfilex(impl_->lua, impl_->package.entrypoint.string().c_str(), "t") !=
      LUA_OK) {
    const std::string message = lua_tostring(impl_->lua, -1);
    lua_pop(impl_->lua, 1);
    throw std::runtime_error("Could not load native game: " + message);
  }
  lua_sethook(impl_->lua, Impl::instruction_limit, LUA_MASKCOUNT, 100000);
  const int load_result = lua_pcall(impl_->lua, 0, 0, 0);
  lua_sethook(impl_->lua, nullptr, 0, 0);
  if (load_result != LUA_OK) {
    const std::string message = lua_tostring(impl_->lua, -1);
    lua_pop(impl_->lua, 1);
    throw std::runtime_error("Could not load native game: " + message);
  }
  impl_->call("init", 0);
  impl_->started = true;
  impl_->events.insert(impl_->events.begin(), {"GameStarted", "fresh"});
}

void Session::step(const Actions& actions) {
  if (!impl_->started) {
    throw std::runtime_error("Native-game session has not started");
  }
  if (impl_->stopped) {
    throw std::runtime_error("Native-game session is stopped");
  }
  lua_newtable(impl_->lua);
  const auto add = [this](const char* name, bool pressed) {
    lua_pushboolean(impl_->lua, pressed);
    lua_setfield(impl_->lua, -2, name);
  };
  add("up", actions.up);
  add("down", actions.down);
  add("left", actions.left);
  add("right", actions.right);
  add("primary", actions.primary);
  add("secondary", actions.secondary);
  add("start", actions.start);
  add("back", actions.back);
  impl_->call("update", 1);
  ++impl_->tick;
}

void Session::stop() {
  if (!impl_->started) {
    throw std::runtime_error("Native-game session has not started");
  }
  if (!impl_->stopped) {
    impl_->events.push_back({"GameExited", "normal"});
    impl_->stopped = true;
  }
}

const std::vector<DrawCommand>& Session::render() {
  if (!impl_->started) {
    throw std::runtime_error("Native-game session has not started");
  }
  impl_->drawing.clear();
  impl_->call("render", 0);
  return impl_->drawing;
}

std::vector<RuntimeEvent> Session::drain_events() {
  std::vector<RuntimeEvent> result;
  result.swap(impl_->events);
  return result;
}

std::string Session::snapshot() const {
  if (!impl_->started) {
    throw std::runtime_error("Native-game session has not started");
  }
  impl_->call("snapshot", 0, 1);
  if (!lua_isstring(impl_->lua, -1)) {
    lua_pop(impl_->lua, 1);
    throw std::runtime_error("Game snapshot should return text");
  }
  const std::string result = lua_tostring(impl_->lua, -1);
  lua_pop(impl_->lua, 1);
  return result;
}

const PackageManifest& Session::package() const noexcept { return impl_->package; }

}  // namespace sprout::runtime
