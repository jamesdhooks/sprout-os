#include "sprout/launcher/framebuffer_page_writer.hpp"

#include <cstdio>

namespace sprout::launcher {

bool write_framebuffer_pages(const std::filesystem::path& path,
                             const std::uint8_t* frame,
                             std::size_t frame_size,
                             std::size_t page_count) {
  if (frame == nullptr || frame_size == 0 || page_count == 0) {
    return false;
  }

  const auto path_string = path.string();
  std::FILE* output = std::fopen(path_string.c_str(), "r+b");
  if (output == nullptr) {
    output = std::fopen(path_string.c_str(), "wb");
  }
  if (output == nullptr) {
    return false;
  }

  bool success = std::fseek(output, 0, SEEK_SET) == 0;
  for (std::size_t page = 0; success && page < page_count; ++page) {
    success = std::fwrite(frame, 1, frame_size, output) == frame_size;
  }
  success = success && std::fflush(output) == 0;
  success = std::fclose(output) == 0 && success;
  return success;
}

}  // namespace sprout::launcher
