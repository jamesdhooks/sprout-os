#pragma once

#include "sprout/runtime/assets.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sprout::runtime {

enum class PackageAudience {
  Family,
  Parent,
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
};

PackageManifest load_package(const std::filesystem::path& package_root);

}  // namespace sprout::runtime
