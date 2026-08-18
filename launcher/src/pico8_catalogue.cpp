#include "sprout/launcher/pico8_catalogue.hpp"
#include <yyjson.h>
#include <fstream>
#include <string_view>

namespace sprout::launcher {
Pico8Catalogue::Pico8Catalogue(std::filesystem::path root) : sd_card_root_(std::move(root)) {}

namespace {

std::optional<Pico8CatalogueEntry> find_in_catalogue(
    const std::filesystem::path& sd_card_root,
    const std::filesystem::path& catalogue_path,
    std::string_view platform,
    const std::filesystem::path& relative) {
  std::ifstream file(catalogue_path, std::ios::binary);
  if (!file) return std::nullopt;
  const std::string source{std::istreambuf_iterator<char>(file), {}};
  yyjson_doc* document = yyjson_read(source.data(), source.size(), 0);
  if (!document) return std::nullopt;
  const auto cleanup = [&] { yyjson_doc_free(document); };
  auto* root = yyjson_doc_get_root(document);
  auto* entries = root ? yyjson_obj_get(root, "entries") : nullptr;
  if (!yyjson_is_arr(entries)) { cleanup(); return std::nullopt; }
  yyjson_arr_iter it = yyjson_arr_iter_with(entries);
  const auto wanted = relative.generic_string();
  while (auto* item = yyjson_arr_iter_next(&it)) {
    auto* item_platform = yyjson_obj_get(item, "platform");
    auto* cart = yyjson_obj_get(item, "cart");
    auto* id = yyjson_obj_get(item, "id");
    auto* title = yyjson_obj_get(item, "title");
    auto* cover = yyjson_obj_get(item, "cover");
    const bool platform_matches =
        platform.empty() || !yyjson_is_str(item_platform) ||
        platform == std::string_view(yyjson_get_str(item_platform),
                                     yyjson_get_len(item_platform));
    if (platform_matches && yyjson_is_str(cart) && yyjson_is_str(id) &&
        yyjson_is_str(title) && wanted == yyjson_get_str(cart)) {
      Pico8CatalogueEntry entry{
          std::string(yyjson_get_str(id), yyjson_get_len(id)),
          std::string(yyjson_get_str(title), yyjson_get_len(title)), {}};
      if (yyjson_is_str(cover)) {
        entry.artwork_path =
            sd_card_root /
            std::string(yyjson_get_str(cover), yyjson_get_len(cover));
      }
      cleanup(); return entry;
    }
  }
  cleanup(); return std::nullopt;
}

}  // namespace

std::optional<Pico8CatalogueEntry> Pico8Catalogue::find(
    std::string_view platform,
    const std::filesystem::path& relative) const {
  if (const auto shared = find_in_catalogue(
          sd_card_root_, sd_card_root_ / "Sprout/catalogue/games.json",
          platform, relative)) {
    return shared;
  }
  if (platform == "PICO") {
    return find_in_catalogue(sd_card_root_,
                             sd_card_root_ / "Sprout/catalogue/pico8.json",
                             platform, relative);
  }
  return std::nullopt;
}

std::optional<Pico8CatalogueEntry> Pico8Catalogue::find(
    const std::filesystem::path& relative) const {
  return find("PICO", relative);
}
}  // namespace sprout::launcher
