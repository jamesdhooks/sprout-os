#pragma once

#include "sprout/runtime/assets.hpp"

#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace sprout::runtime {

enum class PackageAudience {
  Family,
  Parent,
};

enum class PresentationFit {
  Cover,
  Contain,
};

struct NormalizedRegion {
  int x{};
  int y{};
  int width{};
  int height{};
};

struct TitleScreen {
  bool enabled{};
  std::filesystem::path image;
  int image_width{};
  int image_height{};
  PresentationFit fit{PresentationFit::Cover};
  NormalizedRegion title_region;
  NormalizedRegion controls_region;
  std::array<std::uint8_t, 4> controls_background{255, 249, 225, 235};
  std::array<std::uint8_t, 4> controls_foreground{37, 67, 53, 255};
};

struct LibraryArtwork {
  bool enabled{};
  std::filesystem::path image;
  int image_width{};
  int image_height{};
  PresentationFit fit{PresentationFit::Cover};
};

struct PackageSound {
  std::string id;
  std::filesystem::path file;
};

struct PackageManifest {
  std::uint32_t schema_version{};
  std::string id;
  std::string title;
  std::string version;
  std::uint32_t runtime_version{};
  std::filesystem::path root;
  std::filesystem::path entrypoint;
  int logical_width{};
  int logical_height{};
  PackageAudience audience{PackageAudience::Parent};
  std::vector<std::string> capabilities;
  AssetCatalogue assets;
  TitleScreen title_screen;
  LibraryArtwork library_artwork;
  std::vector<PackageSound> sounds;
};

PackageManifest load_package(const std::filesystem::path& package_root);

}  // namespace sprout::runtime
