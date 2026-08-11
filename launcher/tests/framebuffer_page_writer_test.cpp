#include "sprout/launcher/framebuffer_page_writer.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>

int main() {
  std::fprintf(stderr, "checkpoint=begin\n");
  const auto path = std::filesystem::temp_directory_path() /
                    "sprout-framebuffer-page-writer-test.bin";
  std::filesystem::remove(path);
  std::fprintf(stderr, "checkpoint=path\n");

  const std::array<std::uint8_t, 4> frame{0x12, 0x34, 0x56, 0x78};
  if (!sprout::launcher::write_framebuffer_pages(
          path, frame.data(), frame.size(), 2)) {
    return 1;
  }
  std::fprintf(stderr, "checkpoint=written\n");

  std::array<std::uint8_t, 8> actual{};
  {
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(actual.data()),
               static_cast<std::streamsize>(actual.size()));
    if (input.gcount() != static_cast<std::streamsize>(actual.size())) {
      return 2;
    }
  }
  const std::array<std::uint8_t, 8> expected{
      0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78};
  if (actual != expected) {
    return 3;
  }
  std::fprintf(stderr, "checkpoint=verified\n");

  std::filesystem::remove(path);
  std::fprintf(stderr, "checkpoint=done\n");
  return 0;
}
