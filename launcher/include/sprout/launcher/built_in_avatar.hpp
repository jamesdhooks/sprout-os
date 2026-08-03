#pragma once

#include "sprout/launcher/read_only_view.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace sprout::launcher {

struct BuiltInAvatar {
  std::string_view id;
  std::string_view label;
};

[[nodiscard]] ReadOnlyView<BuiltInAvatar> built_in_avatars() noexcept;
[[nodiscard]] const BuiltInAvatar* find_built_in_avatar(
    std::string_view avatar_ref) noexcept;
[[nodiscard]] std::string built_in_avatar_ref(std::string_view id);
[[nodiscard]] std::filesystem::path built_in_avatar_master_path(
    const std::filesystem::path& avatar_root, std::string_view id);
[[nodiscard]] std::filesystem::path built_in_avatar_thumbnail_path(
    const std::filesystem::path& avatar_root, std::string_view id);

}  // namespace sprout::launcher
