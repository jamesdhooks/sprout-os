#pragma once

#include "sprout/launcher/launcher_state.hpp"
#include "sprout/launcher/profile_image_importer.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

enum class ProfileImageCropEvent {
  Imported,
  Cancelled,
};

class ProfileImageCropPresentation {
 public:
  ProfileImageCropPresentation(ProfileImageImporter& importer, std::string profile_id,
                               const std::filesystem::path& source_path);

  [[nodiscard]] CropSelection selection() const noexcept;
  [[nodiscard]] std::vector<std::uint8_t> preview_rgba() const;
  [[nodiscard]] const std::string& error_message() const noexcept;
  [[nodiscard]] std::optional<ProfileImageCropEvent> handle(Action action);

 private:
  ProfileImageImporter& importer_;
  std::string profile_id_;
  ProfileImageCropSession session_;
  std::string error_message_;
};

}  // namespace sprout::launcher
