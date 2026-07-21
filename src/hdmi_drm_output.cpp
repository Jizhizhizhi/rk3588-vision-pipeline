#include "vision/hdmi_output.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace vision {
namespace {

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

}  // namespace

DrmHdmiOutput::DrmHdmiOutput(std::string card) : card_(std::move(card)) {}

DrmHdmiOutput::~DrmHdmiOutput() {
    close();
}

void DrmHdmiOutput::open() {
    if (fd_ >= 0) {
        return;
    }
    fd_ = ::open(card_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        throw_errno("open DRM card");
    }

    drmModeRes* resources = drmModeGetResources(fd_);
    if (!resources) {
        close();
        throw_errno("drmModeGetResources");
    }

    drmModeConnector* selected = nullptr;
    drmModeEncoder* encoder = nullptr;
    for (int i = 0; i < resources->count_connectors && !selected; ++i) {
        drmModeConnector* connector =
            drmModeGetConnector(fd_, resources->connectors[i]);
        if (connector && connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0) {
            selected = connector;
        } else if (connector) {
            drmModeFreeConnector(connector);
        }
    }
    if (!selected) {
        drmModeFreeResources(resources);
        close();
        throw std::runtime_error("no connected DRM connector");
    }

    connector_id_ = selected->connector_id;
    int mode_index = 0;
    for (int i = 0; i < selected->count_modes; ++i) {
        if (selected->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            mode_index = i;
            break;
        }
    }
    auto* selected_mode = new drmModeModeInfo(selected->modes[mode_index]);
    mode_ = selected_mode;
    width_ = selected_mode->hdisplay;
    height_ = selected_mode->vdisplay;

    if (selected->encoder_id) {
        encoder = drmModeGetEncoder(fd_, selected->encoder_id);
    }
    if (encoder && encoder->crtc_id) {
        crtc_id_ = encoder->crtc_id;
    } else {
        for (int i = 0; i < selected->count_encoders && !crtc_id_; ++i) {
            drmModeEncoder* candidate =
                drmModeGetEncoder(fd_, selected->encoders[i]);
            if (!candidate) {
                continue;
            }
            for (int c = 0; c < resources->count_crtcs; ++c) {
                if (candidate->possible_crtcs & (1U << c)) {
                    crtc_id_ = resources->crtcs[c];
                    break;
                }
            }
            drmModeFreeEncoder(candidate);
        }
    }
    if (!crtc_id_) {
        if (encoder) drmModeFreeEncoder(encoder);
        drmModeFreeConnector(selected);
        drmModeFreeResources(resources);
        close();
        throw std::runtime_error("no usable DRM CRTC");
    }

    saved_crtc_ = drmModeGetCrtc(fd_, crtc_id_);

    drm_mode_create_dumb create{};
    create.width = width_;
    create.height = height_;
    create.bpp = 32;
    if (::ioctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        throw_errno("DRM_IOCTL_MODE_CREATE_DUMB");
    }
    handle_ = create.handle;
    pitch_ = create.pitch;
    size_ = create.size;

    if (drmModeAddFB(fd_, width_, height_, 24, 32, pitch_, handle_,
                     &framebuffer_id_) != 0) {
        throw_errno("drmModeAddFB");
    }

    drm_mode_map_dumb map{};
    map.handle = handle_;
    if (::ioctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        throw_errno("DRM_IOCTL_MODE_MAP_DUMB");
    }
    mapped_ = static_cast<std::uint8_t*>(::mmap(
        nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map.offset));
    if (mapped_ == MAP_FAILED) {
        mapped_ = nullptr;
        throw_errno("mmap DRM dumb buffer");
    }
    std::memset(mapped_, 0, size_);

    if (drmModeSetCrtc(fd_, crtc_id_, framebuffer_id_, 0, 0,
                       &connector_id_, 1, selected_mode) != 0) {
        throw_errno("drmModeSetCrtc");
    }

    if (encoder) drmModeFreeEncoder(encoder);
    drmModeFreeConnector(selected);
    drmModeFreeResources(resources);
}

void DrmHdmiOutput::close() {
    if (fd_ < 0) {
        return;
    }
    auto* saved = static_cast<drmModeCrtc*>(saved_crtc_);
    if (saved) {
        drmModeSetCrtc(fd_, saved->crtc_id, saved->buffer_id,
                       saved->x, saved->y, &connector_id_, 1, &saved->mode);
        drmModeFreeCrtc(saved);
    }
    saved_crtc_ = nullptr;
    if (mapped_) {
        ::munmap(mapped_, size_);
        mapped_ = nullptr;
    }
    if (framebuffer_id_) {
        drmModeRmFB(fd_, framebuffer_id_);
        framebuffer_id_ = 0;
    }
    if (handle_) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = handle_;
        ::ioctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        handle_ = 0;
    }
    delete static_cast<drmModeModeInfo*>(mode_);
    mode_ = nullptr;
    ::close(fd_);
    fd_ = -1;
}

void DrmHdmiOutput::present_xrgb8888(
    const std::uint8_t* pixels, std::size_t bytes, std::uint32_t width,
    std::uint32_t height, std::uint32_t stride) {
    if (!mapped_) {
        throw std::logic_error("DRM output is not open");
    }
    if (width != width_ || height != height_) {
        throw std::invalid_argument("frame size does not match DRM mode");
    }
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4;
    if (stride < row_bytes || bytes < static_cast<std::size_t>(stride) * height) {
        throw std::invalid_argument("invalid XRGB8888 frame buffer");
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(mapped_ + static_cast<std::size_t>(y) * pitch_,
                    pixels + static_cast<std::size_t>(y) * stride, row_bytes);
    }
}

std::uint32_t DrmHdmiOutput::width() const { return width_; }
std::uint32_t DrmHdmiOutput::height() const { return height_; }

}  // namespace vision
