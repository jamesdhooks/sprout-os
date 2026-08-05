#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace sprout::launcher {
struct Pico8CatalogueEntry { std::string id; std::string title; std::filesystem::path artwork_path; };
class Pico8Catalogue {
 public:
  explicit Pico8Catalogue(std::filesystem::path sd_card_root);
  [[nodiscard]] std::optional<Pico8CatalogueEntry> find(
      const std::filesystem::path& cart_relative_path) const;
 private:
  std::filesystem::path sd_card_root_;
};
}  // namespace sprout::launcher
