#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vision/frame.hpp"

namespace vision {

class V4L2Capture {
public:
    V4L2Capture(std::string device, std::uint32_t width,
                std::uint32_t height, std::uint32_t fps);
    ~V4L2Capture();

    V4L2Capture(const V4L2Capture&) = delete;
    V4L2Capture& operator=(const V4L2Capture&) = delete;

    void open();
    void close();
    Frame read(int timeout_ms = 1000);

private:
    struct Buffer {
        void* start = nullptr;
        std::size_t length = 0;
    };

    std::string device_;
    std::uint32_t width_;
    std::uint32_t height_;
    std::uint32_t fps_;
    std::uint32_t stride_ = 0;
    int fd_ = -1;
    std::uint64_t next_frame_id_ = 1;
    std::vector<Buffer> buffers_;
};

}  // namespace vision
