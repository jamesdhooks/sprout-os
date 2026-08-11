#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>


namespace sprout::launcher {

bool write_framebuffer_pages(const std::filesystem::path& path,
                             const std::uint8_t* frame,
                             std::size_t frame_size,
                             std::size_t page_count);

}  // namespace sprout::launcher
