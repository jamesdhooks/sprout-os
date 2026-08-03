#include "sprout/launcher/profile_image_importer.hpp"
#include "sprout/launcher/string_compat.hpp"
#include "sprout/launcher/profile_image_crop_presentation.hpp"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using sprout::launcher::CropSelection;
using sprout::launcher::NewProfile;
using sprout::launcher::ProfileImageImporter;
using sprout::launcher::ProfileImageCropEvent;
using sprout::launcher::ProfileImageCropPresentation;
using sprout::launcher::ProfileImageCropSession;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Operation>
void expect_failure(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-profile-image-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void create_parent(ProfileRepository& profiles) {
  profiles.create_profile(NewProfile{
      .id = "parent-sam",
      .display_name = "Sam",
      .role = ProfileRole::Parent,
      .avatar_ref = "builtin:fox",
      .save_namespace = "saves-parent-sam",
  });
}

void set_pixel(SDL_Surface& surface, int x, int y, std::uint8_t red,
               std::uint8_t green, std::uint8_t blue) {
  const std::uint32_t pixel = SDL_MapRGBA(surface.format, red, green, blue, 255);
  auto* row = static_cast<std::uint8_t*>(surface.pixels) + y * surface.pitch;
  *reinterpret_cast<std::uint32_t*>(row + x * 4) = pixel;
}

void create_two_color_source(const std::filesystem::path& path, bool jpeg) {
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, 8, 4, 32, SDL_PIXELFORMAT_RGBA32);
  if (surface == nullptr) {
    throw std::runtime_error("Could not create image fixture");
  }
  for (int y = 0; y < surface->h; ++y) {
    for (int x = 0; x < surface->w; ++x) {
      if (x < surface->w / 2) {
        set_pixel(*surface, x, y, 240, 20, 20);
      } else {
        set_pixel(*surface, x, y, 20, 20, 240);
      }
    }
  }
  const std::string encoded = path_as_utf8(path);
  const int result = jpeg ? IMG_SaveJPG(surface, encoded.c_str(), 100)
                          : IMG_SavePNG(surface, encoded.c_str());
  SDL_FreeSurface(surface);
  if (result != 0) {
    throw std::runtime_error(std::string("Could not save image fixture: ") +
                             IMG_GetError());
  }
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void add_orientation_six(const std::filesystem::path& jpeg_path) {
  auto jpeg = read_bytes(jpeg_path);
  expect(jpeg.size() > 2 && jpeg[0] == 0xFF && jpeg[1] == 0xD8,
         "JPEG fixture should start with SOI");
  const std::vector<std::uint8_t> exif{
      0xFF, 0xE1, 0x00, 0x22, 'E',  'x',  'i',  'f',  0x00, 0x00,
      'M',  'M',  0x00, 0x2A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01,
      0x01, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  jpeg.insert(jpeg.begin() + 2, exif.begin(), exif.end());
  write_bytes(jpeg_path, jpeg);
}

std::array<std::uint8_t, 3> pixel_rgb(const std::filesystem::path& path, int x,
                                      int y) {
  const std::string encoded = path_as_utf8(path);
  SDL_Surface* loaded = IMG_Load(encoded.c_str());
  if (loaded == nullptr) {
    throw std::runtime_error("Could not load managed test image");
  }
  SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded);
  if (rgba == nullptr) {
    throw std::runtime_error("Could not normalize managed test image");
  }
  const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + y * rgba->pitch;
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;
  std::uint32_t value = 0;
  std::copy_n(row + x * 4, 4, reinterpret_cast<std::uint8_t*>(&value));
  SDL_GetRGBA(value, rgba->format, &red, &green, &blue, &alpha);
  SDL_FreeSurface(rgba);
  return {red, green, blue};
}

void imports_managed_variants_and_updates_profile() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  create_parent(profiles);
  const auto source = directory.path() / "source.png";
  create_two_color_source(source, false);

  ProfileImageImporter importer(directory.path() / "images", profiles);
  const auto imported = importer.import_for_profile("parent-sam", source);
  expect(sprout::launcher::starts_with(imported.avatar_ref, "local:"),
         "import should produce a managed local reference");
  expect(std::filesystem::is_regular_file(imported.portrait_path),
         "import should write a managed portrait");
  expect(std::filesystem::is_regular_file(imported.thumbnail_path),
         "import should write a managed thumbnail");
  expect(importer.resolve_portrait(imported.avatar_ref) == imported.portrait_path,
         "managed reference should resolve without a source path");
  const auto profile = profiles.find_profile("parent-sam");
  expect(profile->avatar_ref == imported.avatar_ref,
         "profile should reference the activated managed image");
  expect(profile->avatar_ref.find("source.png") == std::string::npos,
         "profile row should not retain the source path");

  SDL_Surface* portrait = IMG_Load(path_as_utf8(imported.portrait_path).c_str());
  SDL_Surface* thumbnail = IMG_Load(path_as_utf8(imported.thumbnail_path).c_str());
  expect(portrait != nullptr && portrait->w == 256 && portrait->h == 256,
         "portrait variant should be 256 square");
  expect(thumbnail != nullptr && thumbnail->w == 96 && thumbnail->h == 96,
         "thumbnail variant should be 96 square");
  SDL_FreeSurface(portrait);
  SDL_FreeSurface(thumbnail);
}

void honors_jpeg_exif_orientation_and_strips_metadata() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  create_parent(profiles);
  const auto source = directory.path() / "rotated.jpg";
  create_two_color_source(source, true);
  add_orientation_six(source);

  ProfileImageImporter importer(directory.path() / "images", profiles);
  const auto imported = importer.import_for_profile("parent-sam", source);
  const auto top = pixel_rgb(imported.portrait_path, 128, 20);
  const auto bottom = pixel_rgb(imported.portrait_path, 128, 235);
  expect(top[0] > top[2] && bottom[2] > bottom[0],
         "orientation six should rotate left/right colors to top/bottom");

  const auto managed_bytes = read_bytes(imported.portrait_path);
  const std::string managed(managed_bytes.begin(), managed_bytes.end());
  expect(managed.find("Exif") == std::string::npos,
         "managed PNG should not retain EXIF metadata");
}

void rejects_bad_inputs_without_replacing_portrait() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  create_parent(profiles);
  ProfileImageImporter importer(directory.path() / "images", profiles);

  const auto text = directory.path() / "not-image.txt";
  write_bytes(text, {'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'});
  expect_failure([&] { (void)importer.import_for_profile("parent-sam", text); },
                 "unsupported input should be rejected");
  expect_failure(
      [&] { (void)importer.import_for_profile("parent-sam", directory.path() / "missing"); },
      "missing input should be rejected");

  const auto oversized = directory.path() / "oversized.bin";
  {
    std::ofstream stream(oversized, std::ios::binary | std::ios::trunc);
    stream.seekp(16 * 1024 * 1024);
    stream.put('\0');
  }
  expect_failure([&] { (void)importer.import_for_profile("parent-sam", oversized); },
                 "source larger than 16 MiB should be rejected before decode");

  const auto valid = directory.path() / "valid.png";
  create_two_color_source(valid, false);
  const auto first = importer.import_for_profile("parent-sam", valid);
  const auto reference = profiles.find_profile("parent-sam")->avatar_ref;
  expect_failure(
      [&] {
        (void)importer.import_for_profile(
            "parent-sam", valid,
            CropSelection{.center_x = 2.0, .center_y = 0.5, .zoom = 1.0});
      },
      "invalid crop should be rejected");
  expect(profiles.find_profile("parent-sam")->avatar_ref == reference,
         "failed crop should preserve the current portrait reference");
  expect(std::filesystem::is_regular_file(first.portrait_path),
         "failed crop should preserve the current managed portrait");
}

void replacement_removes_only_the_prior_managed_generation() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  create_parent(profiles);
  const auto source = directory.path() / "source.png";
  create_two_color_source(source, false);
  ProfileImageImporter importer(directory.path() / "images", profiles);
  const auto first = importer.import_for_profile("parent-sam", source);
  const auto second = importer.import_for_profile(
      "parent-sam", source,
      CropSelection{.center_x = 0.25, .center_y = 0.5, .zoom = 2.0});

  expect(first.avatar_ref != second.avatar_ref,
         "replacement should activate a new profile revision generation");
  expect(!std::filesystem::exists(first.portrait_path.parent_path()),
         "successful replacement should remove the prior managed generation");
  expect(std::filesystem::is_regular_file(second.portrait_path),
         "replacement should retain the new managed portrait");
  expect_failure([&] { (void)importer.resolve_portrait("local:../escape"); },
                 "managed path resolution should reject traversal");
}

void crop_session_is_bounded_and_presentation_imports() {
  TemporaryDirectory directory;
  ProfileRepository profiles(directory.path() / "profiles.sqlite3");
  create_parent(profiles);
  const auto source = directory.path() / "source.png";
  create_two_color_source(source, false);

  ProfileImageCropSession session(source);
  expect(session.preview_rgba().size() == 256U * 256U * 4U,
         "crop preview should expose a 256-square RGBA frame");
  for (int index = 0; index < 20; ++index) {
    session.move(-0.1, 0.1);
    session.adjust_zoom(0.25);
  }
  const auto bounded = session.selection();
  expect(bounded.center_x == 0.0 && bounded.center_y == 1.0 && bounded.zoom == 4.0,
         "crop controls should clamp to supported bounds");

  ProfileImageImporter importer(directory.path() / "images", profiles);
  ProfileImageCropPresentation cancelled(importer, "parent-sam", source);
  expect(cancelled.handle(sprout::launcher::Action::Back) ==
             ProfileImageCropEvent::Cancelled,
         "back should cancel without importing");
  expect(profiles.find_profile("parent-sam")->avatar_ref == "builtin:fox",
         "cancel should preserve the built-in portrait");

  ProfileImageCropPresentation accepted(importer, "parent-sam", source);
  (void)accepted.handle(sprout::launcher::Action::ZoomIn);
  (void)accepted.handle(sprout::launcher::Action::Left);
  expect(accepted.handle(sprout::launcher::Action::Confirm) ==
             ProfileImageCropEvent::Imported,
         "confirm should import the visible crop");
  expect(sprout::launcher::starts_with(
             profiles.find_profile("parent-sam")->avatar_ref, "local:"),
         "crop confirmation should persist a managed reference");
}

}  // namespace

int main() {
  if (SDL_Init(0) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }
  try {
    imports_managed_variants_and_updates_profile();
    honors_jpeg_exif_orientation_and_strips_metadata();
    rejects_bad_inputs_without_replacing_portrait();
    replacement_removes_only_the_prior_managed_generation();
    crop_session_is_bounded_and_presentation_imports();
  } catch (const std::exception& error) {
    std::cerr << "profile image importer test failed: " << error.what() << '\n';
    SDL_Quit();
    return EXIT_FAILURE;
  }
  SDL_Quit();
  std::cout << "profile image importer tests passed\n";
  return EXIT_SUCCESS;
}
