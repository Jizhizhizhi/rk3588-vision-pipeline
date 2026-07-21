#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace vision {

enum class PixelFormat {
    kYuyv422,
    kXrgb8888,
};

struct Frame {
    std::uint64_t frame_id = 0;
    std::uint64_t timestamp_ns = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    PixelFormat format = PixelFormat::kYuyv422;
    std::shared_ptr<std::vector<std::uint8_t>> bytes;
};

}  // namespace vision
