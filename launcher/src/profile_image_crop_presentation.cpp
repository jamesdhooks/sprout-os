#include "sprout/launcher/profile_image_crop_presentation.hpp"

#include <exception>
#include <utility>

namespace sprout::launcher {

ProfileImageCropPresentation::ProfileImageCropPresentation(
    ProfileImageImporter& importer, std::string profile_id,
    const std::filesystem::path& source_path)
    : importer_(importer),
      profile_id_(std::move(profile_id)),
      session_(source_path) {}

CropSelection ProfileImageCropPresentation::selection() const noexcept {
  return session_.selection();
}

std::vector<std::uint8_t> ProfileImageCropPresentation::preview_rgba() const {
  return session_.preview_rgba();
}

const std::string& ProfileImageCropPresentation::error_message() const noexcept {
  return error_message_;
}

std::optional<ProfileImageCropEvent> ProfileImageCropPresentation::handle(Action action) {
  error_message_.clear();
  switch (action) {
    case Action::Up: session_.move(0.0, -0.05); return std::nullopt;
    case Action::Down: session_.move(0.0, 0.05); return std::nullopt;
    case Action::Left: session_.move(-0.05, 0.0); return std::nullopt;
    case Action::Right: session_.move(0.05, 0.0); return std::nullopt;
    case Action::ZoomIn: session_.adjust_zoom(0.25); return std::nullopt;
    case Action::ZoomOut: session_.adjust_zoom(-0.25); return std::nullopt;
    case Action::Back: return ProfileImageCropEvent::Cancelled;
    case Action::Confirm:
      try {
        (void)importer_.import_for_profile(profile_id_, session_);
        return ProfileImageCropEvent::Imported;
      } catch (const std::exception& error) {
        error_message_ = error.what();
        return std::nullopt;
      }
  }
  return std::nullopt;
}

}  // namespace sprout::launcher
