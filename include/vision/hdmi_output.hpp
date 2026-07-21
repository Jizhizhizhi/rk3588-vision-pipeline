#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace vision {

class DrmHdmiOutput {
public:
    explicit DrmHdmiOutput(std::string card = "/dev/dri/card0");
    ~DrmHdmiOutput();

    DrmHdmiOutput(const DrmHdmiOutput&) = delete;
    DrmHdmiOutput& operator=(const DrmHdmiOutput&) = delete;

    void open();
    void close();
    void present_xrgb8888(const std::uint8_t* pixels, std::size_t bytes,
                          std::uint32_t width, std::uint32_t height,
                          std::uint32_t stride);

    std::uint32_t width() const;
    std::uint32_t height() const;

private:
    std::string card_;
    int fd_ = -1;
    std::uint32_t connector_id_ = 0;
    std::uint32_t crtc_id_ = 0;
    std::uint32_t framebuffer_id_ = 0;
    std::uint32_t handle_ = 0;
    std::uint32_t pitch_ = 0;
    std::uint64_t size_ = 0;
    std::uint8_t* mapped_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    void* saved_crtc_ = nullptr;
    void* mode_ = nullptr;
};

}  // namespace vision
