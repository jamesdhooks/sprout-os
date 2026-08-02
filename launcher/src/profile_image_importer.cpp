#include "sprout/launcher/profile_image_importer.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace sprout::launcher {
namespace {

constexpr std::uintmax_t kMaximumSourceBytes = 16U * 1024U * 1024U;
constexpr std::uint64_t kMaximumDecodedPixels = 32U * 1024U * 1024U;
constexpr int kPortraitSize = 256;
constexpr int kThumbnailSize = 96;

struct SurfaceDeleter {
  void operator()(SDL_Surface* surface) const { SDL_FreeSurface(surface); }
};

using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

class SurfaceLock {
 public:
  explicit SurfaceLock(SDL_Surface& surface) : surface_(surface) {
    if (SDL_MUSTLOCK(&surface_) && SDL_LockSurface(&surface_) != 0) {
      throw std::runtime_error("Could not lock profile image pixels");
    }
    locked_ = SDL_MUSTLOCK(&surface_) != 0;
  }

  ~SurfaceLock() {
    if (locked_) {
      SDL_UnlockSurface(&surface_);
    }
  }

  SurfaceLock(const SurfaceLock&) = delete;
  SurfaceLock& operator=(const SurfaceLock&) = delete;

 private:
  SDL_Surface& surface_;
  bool locked_{false};
};

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::vector<std::uint8_t> read_source(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > kMaximumSourceBytes) {
    throw std::invalid_argument("Profile image must be a file between 1 byte and 16 MiB");
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::invalid_argument("Profile image could not be opened");
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  stream.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  if (!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error("Profile image could not be read completely");
  }
  return bytes;
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset,
                       bool little_endian) {
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("Truncated EXIF value");
  }
  if (little_endian) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
  }
  return static_cast<std::uint16_t>(bytes[offset] << 8U) |
         static_cast<std::uint16_t>(bytes[offset + 1]);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset,
                       bool little_endian) {
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("Truncated EXIF value");
  }
  if (little_endian) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
  }
  return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

int exif_orientation(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) {
    return 1;
  }

  std::size_t position = 2;
  while (position + 4 <= bytes.size()) {
    if (bytes[position] != 0xFF) {
      break;
    }
    while (position < bytes.size() && bytes[position] == 0xFF) {
      ++position;
    }
    if (position >= bytes.size()) {
      break;
    }
    const std::uint8_t marker = bytes[position++];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    if (position + 2 > bytes.size()) {
      break;
    }
    const std::size_t length =
        (static_cast<std::size_t>(bytes[position]) << 8U) | bytes[position + 1];
    if (length < 2 || position + length > bytes.size()) {
      break;
    }
    const std::size_t payload = position + 2;
    const std::size_t payload_size = length - 2;
    position += length;
    if (marker != 0xE1 || payload_size < 14 ||
        !std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(payload),
                    bytes.begin() + static_cast<std::ptrdiff_t>(payload + 6),
                    std::array<std::uint8_t, 6>{'E', 'x', 'i', 'f', 0, 0}.begin())) {
      continue;
    }

    const std::size_t tiff = payload + 6;
    const bool little_endian = bytes[tiff] == 'I' && bytes[tiff + 1] == 'I';
    const bool big_endian = bytes[tiff] == 'M' && bytes[tiff + 1] == 'M';
    if ((!little_endian && !big_endian) || read_u16(bytes, tiff + 2, little_endian) != 42) {
      return 1;
    }
    const std::uint32_t ifd_offset = read_u32(bytes, tiff + 4, little_endian);
    if (ifd_offset > payload_size || tiff + ifd_offset + 2 > payload + payload_size) {
      return 1;
    }
    const std::size_t ifd = tiff + ifd_offset;
    const std::uint16_t count = read_u16(bytes, ifd, little_endian);
    for (std::uint16_t index = 0; index < count; ++index) {
      const std::size_t entry = ifd + 2 + static_cast<std::size_t>(index) * 12;
      if (entry + 12 > payload + payload_size) {
        return 1;
      }
      if (read_u16(bytes, entry, little_endian) == 0x0112 &&
          read_u16(bytes, entry + 2, little_endian) == 3 &&
          read_u32(bytes, entry + 4, little_endian) == 1) {
        const int orientation = read_u16(bytes, entry + 8, little_endian);
        return orientation >= 1 && orientation <= 8 ? orientation : 1;
      }
    }
    return 1;
  }
  return 1;
}

Surface decode(const std::vector<std::uint8_t>& bytes) {
  SDL_RWops* source = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
  if (source == nullptr) {
    throw std::runtime_error("Could not allocate image reader");
  }
  Surface decoded(IMG_Load_RW(source, 1));
  if (!decoded) {
    throw std::invalid_argument(std::string("Unsupported or malformed profile image: ") +
                                IMG_GetError());
  }
  if (decoded->w <= 0 || decoded->h <= 0 || decoded->w > 8192 || decoded->h > 8192 ||
      static_cast<std::uint64_t>(decoded->w) * decoded->h > kMaximumDecodedPixels) {
    throw std::invalid_argument("Decoded profile image dimensions are too large");
  }
  Surface rgba(SDL_ConvertSurfaceFormat(decoded.get(), SDL_PIXELFORMAT_RGBA32, 0));
  if (!rgba) {
    throw std::runtime_error("Could not normalize profile image pixels");
  }
  return rgba;
}

Surface apply_orientation(const SDL_Surface& source, int orientation) {
  const bool swaps_dimensions = orientation >= 5;
  const int width = swaps_dimensions ? source.h : source.w;
  const int height = swaps_dimensions ? source.w : source.h;
  Surface result(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                SDL_PIXELFORMAT_RGBA32));
  if (!result) {
    throw std::runtime_error("Could not allocate oriented profile image");
  }

  SurfaceLock source_lock(const_cast<SDL_Surface&>(source));
  SurfaceLock result_lock(*result);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int source_x = x;
      int source_y = y;
      switch (orientation) {
        case 2: source_x = source.w - 1 - x; break;
        case 3:
          source_x = source.w - 1 - x;
          source_y = source.h - 1 - y;
          break;
        case 4: source_y = source.h - 1 - y; break;
        case 5:
          source_x = y;
          source_y = x;
          break;
        case 6:
          source_x = y;
          source_y = source.h - 1 - x;
          break;
        case 7:
          source_x = source.w - 1 - y;
          source_y = source.h - 1 - x;
          break;
        case 8:
          source_x = source.w - 1 - y;
          source_y = x;
          break;
        default: break;
      }
      const auto* source_pixel = static_cast<const std::uint8_t*>(source.pixels) +
                                 source_y * source.pitch + source_x * 4;
      auto* destination = static_cast<std::uint8_t*>(result->pixels) + y * result->pitch +
                          x * 4;
      std::copy_n(source_pixel, 4, destination);
    }
  }
  return result;
}

SDL_Rect crop_rectangle(const SDL_Surface& source, CropSelection crop) {
  if (!std::isfinite(crop.center_x) || !std::isfinite(crop.center_y) ||
      !std::isfinite(crop.zoom) || crop.center_x < 0.0 || crop.center_x > 1.0 ||
      crop.center_y < 0.0 || crop.center_y > 1.0 || crop.zoom < 1.0 ||
      crop.zoom > 4.0) {
    throw std::invalid_argument("Crop center must be normalized and zoom must be 1 to 4");
  }
  const int maximum_side = std::min(source.w, source.h);
  const int side = std::max(1, static_cast<int>(std::lround(maximum_side / crop.zoom)));
  const int center_x = static_cast<int>(std::lround(crop.center_x * source.w));
  const int center_y = static_cast<int>(std::lround(crop.center_y * source.h));
  return SDL_Rect{
      .x = std::clamp(center_x - side / 2, 0, source.w - side),
      .y = std::clamp(center_y - side / 2, 0, source.h - side),
      .w = side,
      .h = side,
  };
}

Surface scaled_crop(SDL_Surface& source, const SDL_Rect& crop, int size) {
  Surface output(
      SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32));
  if (!output || SDL_SoftStretchLinear(&source, &crop, output.get(), nullptr) != 0) {
    throw std::runtime_error("Could not scale profile image crop");
  }
  return output;
}

std::string asset_id(const ProfileRecord& profile) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(profile.id.size() * 2 + 24);
  for (const unsigned char value : profile.id) {
    result.push_back(kHex[value >> 4U]);
    result.push_back(kHex[value & 0x0FU]);
  }
  result += "-r" + std::to_string(profile.local_revision + 1);
  return result;
}

bool safe_asset_id(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f') || character == '-' || character == 'r';
  });
}

void synchronize_file(const std::filesystem::path& path) {
  FILE* file = nullptr;
#ifdef _WIN32
  if (_wfopen_s(&file, path.c_str(), L"rb+") != 0) {
#else
  file = std::fopen(path.c_str(), "rb+");
  if (file == nullptr) {
#endif
    throw std::runtime_error("Could not reopen managed image for synchronization");
  }
#ifdef _WIN32
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
  const bool synchronized =
      handle != INVALID_HANDLE_VALUE && FlushFileBuffers(handle) != 0;
#else
  const bool synchronized = fsync(fileno(file)) == 0;
#endif
  const bool closed = std::fclose(file) == 0;
  if (!synchronized || !closed) {
    throw std::runtime_error("Could not synchronize managed image");
  }
}

void activate_file(const std::filesystem::path& pending,
                   const std::filesystem::path& destination) {
#ifdef _WIN32
  if (!MoveFileExW(pending.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error("Could not activate managed image");
  }
#else
  if (std::rename(pending.c_str(), destination.c_str()) != 0) {
    throw std::runtime_error("Could not activate managed image");
  }
#endif
}

void save_png(SDL_Surface& surface, const std::filesystem::path& destination) {
  std::filesystem::path pending = destination;
  pending += ".pending";
  const std::string encoded = path_as_utf8(pending);
  if (IMG_SavePNG(&surface, encoded.c_str()) != 0) {
    throw std::runtime_error(std::string("Could not encode managed PNG: ") + IMG_GetError());
  }
  std::filesystem::permissions(
      pending,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
  synchronize_file(pending);
  activate_file(pending, destination);
}

void remove_new_asset(const std::filesystem::path& directory) {
  std::error_code ignored;
  std::filesystem::remove(directory / "portrait.png.pending", ignored);
  std::filesystem::remove(directory / "thumbnail.png.pending", ignored);
  std::filesystem::remove(directory / "portrait.png", ignored);
  std::filesystem::remove(directory / "thumbnail.png", ignored);
  std::filesystem::remove(directory, ignored);
}

void remove_old_asset(const std::filesystem::path& root, std::string_view reference) {
  if (!reference.starts_with("local:")) {
    return;
  }
  const std::string_view id = reference.substr(6);
  if (!safe_asset_id(id)) {
    return;
  }
  const std::filesystem::path directory = root / id;
  std::error_code error;
  if (std::filesystem::symlink_status(directory, error).type() !=
      std::filesystem::file_type::directory) {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    const auto filename = entry.path().filename();
    if (error || (filename != "portrait.png" && filename != "thumbnail.png") ||
        !entry.is_regular_file(error)) {
      return;
    }
  }
  std::filesystem::remove(directory / "portrait.png", error);
  if (error) {
    return;
  }
  std::filesystem::remove(directory / "thumbnail.png", error);
  if (error) {
    return;
  }
  std::filesystem::remove(directory, error);
}

}  // namespace

ProfileImageImporter::ProfileImageImporter(std::filesystem::path managed_image_root,
                                           ProfileRepository& profiles)
    : root_(std::move(managed_image_root)), profiles_(profiles) {}

ManagedProfileImage ProfileImageImporter::import_for_profile(
    const std::string& profile_id, const std::filesystem::path& source_path,
    CropSelection crop) {
  const auto profile = profiles_.find_profile(profile_id);
  if (!profile.has_value() || profile->lifecycle != ProfileLifecycle::Active) {
    throw std::invalid_argument("Profile image requires an active profile");
  }
  if (profile->local_revision == std::numeric_limits<std::uint64_t>::max()) {
    throw std::runtime_error("Profile revision is exhausted");
  }

  const auto bytes = read_source(source_path);
  Surface decoded = decode(bytes);
  Surface oriented = apply_orientation(*decoded, exif_orientation(bytes));
  const SDL_Rect crop_area = crop_rectangle(*oriented, crop);
  Surface portrait = scaled_crop(*oriented, crop_area, kPortraitSize);
  const SDL_Rect portrait_area{0, 0, kPortraitSize, kPortraitSize};
  Surface thumbnail = scaled_crop(*portrait, portrait_area, kThumbnailSize);

  const std::string id = asset_id(*profile);
  const std::string reference = "local:" + id;
  const std::filesystem::path directory = root_ / id;
  std::filesystem::create_directories(root_);
  if (!std::filesystem::create_directory(directory)) {
    throw std::runtime_error("Managed profile image generation already exists");
  }
  const std::filesystem::path portrait_path = directory / "portrait.png";
  const std::filesystem::path thumbnail_path = directory / "thumbnail.png";
  try {
    save_png(*portrait, portrait_path);
    save_png(*thumbnail, thumbnail_path);
    profiles_.set_avatar_ref(profile_id, reference);
  } catch (...) {
    remove_new_asset(directory);
    throw;
  }
  remove_old_asset(root_, profile->avatar_ref);
  return ManagedProfileImage{
      .avatar_ref = reference,
      .portrait_path = portrait_path,
      .thumbnail_path = thumbnail_path,
  };
}

std::filesystem::path ProfileImageImporter::resolve_portrait(
    const std::string& avatar_ref) const {
  return resolve(avatar_ref, "portrait.png");
}

std::filesystem::path ProfileImageImporter::resolve_thumbnail(
    const std::string& avatar_ref) const {
  return resolve(avatar_ref, "thumbnail.png");
}

std::filesystem::path ProfileImageImporter::resolve(
    const std::string& avatar_ref, const char* filename) const {
  if (!avatar_ref.starts_with("local:") ||
      !safe_asset_id(std::string_view(avatar_ref).substr(6))) {
    throw std::invalid_argument("Avatar reference is not a managed local image");
  }
  return root_ / std::string_view(avatar_ref).substr(6) / filename;
}

}  // namespace sprout::launcher
