#include "vision/capture.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace vision {
namespace {

int xioctl(int fd, unsigned long request, void* arg) {
    int rc;
    do {
        rc = ::ioctl(fd, request, arg);
    } while (rc < 0 && errno == EINTR);
    return rc;
}

std::uint64_t monotonic_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

}  // namespace

V4L2Capture::V4L2Capture(std::string device, std::uint32_t width,
                         std::uint32_t height, std::uint32_t fps)
    : device_(std::move(device)), width_(width), height_(height), fps_(fps) {}

V4L2Capture::~V4L2Capture() {
    close();
}

void V4L2Capture::open() {
    if (fd_ >= 0) {
        return;
    }
    fd_ = ::open(device_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        throw_errno("open V4L2 device");
    }

    try {
        v4l2_capability caps{};
        if (xioctl(fd_, VIDIOC_QUERYCAP, &caps) < 0) {
            throw_errno("VIDIOC_QUERYCAP");
        }
        if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
            !(caps.capabilities & V4L2_CAP_STREAMING)) {
            throw std::runtime_error("device lacks capture/streaming capability");
        }

        v4l2_format format{};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = width_;
        format.fmt.pix.height = height_;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        format.fmt.pix.field = V4L2_FIELD_NONE;
        if (xioctl(fd_, VIDIOC_S_FMT, &format) < 0) {
            throw_errno("VIDIOC_S_FMT");
        }
        if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
            throw std::runtime_error("device did not accept YUYV format");
        }
        width_ = format.fmt.pix.width;
        height_ = format.fmt.pix.height;
        stride_ = format.fmt.pix.bytesperline;
        if (stride_ < width_ * 2) {
            throw std::runtime_error("invalid YUYV bytesperline");
        }

        v4l2_streamparm parm{};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = fps_;
        xioctl(fd_, VIDIOC_S_PARM, &parm);

        v4l2_requestbuffers request{};
        request.count = 4;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd_, VIDIOC_REQBUFS, &request) < 0) {
            throw_errno("VIDIOC_REQBUFS");
        }
        if (request.count < 2) {
            throw std::runtime_error("insufficient V4L2 buffers");
        }

        buffers_.resize(request.count);
        for (std::uint32_t i = 0; i < request.count; ++i) {
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            if (xioctl(fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
                throw_errno("VIDIOC_QUERYBUF");
            }
            buffers_[i].length = buffer.length;
            buffers_[i].start = ::mmap(nullptr, buffer.length,
                                       PROT_READ | PROT_WRITE, MAP_SHARED,
                                       fd_, buffer.m.offset);
            if (buffers_[i].start == MAP_FAILED) {
                buffers_[i].start = nullptr;
                throw_errno("mmap V4L2 buffer");
            }
            if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
                throw_errno("VIDIOC_QBUF");
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
            throw_errno("VIDIOC_STREAMON");
        }
    } catch (...) {
        close();
        throw;
    }
}

void V4L2Capture::close() {
    if (fd_ < 0) {
        return;
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd_, VIDIOC_STREAMOFF, &type);
    for (auto& buffer : buffers_) {
        if (buffer.start) {
            ::munmap(buffer.start, buffer.length);
        }
    }
    buffers_.clear();
    ::close(fd_);
    fd_ = -1;
}

Frame V4L2Capture::read(int timeout_ms) {
    if (fd_ < 0) {
        throw std::logic_error("capture device is not open");
    }
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;
    const int poll_rc = ::poll(&pfd, 1, timeout_ms);
    if (poll_rc == 0) {
        throw std::runtime_error("V4L2 capture timeout");
    }
    if (poll_rc < 0) {
        throw_errno("poll V4L2 device");
    }

    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_DQBUF, &buffer) < 0) {
        throw_errno("VIDIOC_DQBUF");
    }

    Frame frame;
    try {
        frame.frame_id = next_frame_id_++;
        frame.timestamp_ns = monotonic_ns();
        frame.width = width_;
        frame.height = height_;
        frame.stride = stride_;
        frame.format = PixelFormat::kYuyv422;
        frame.bytes = std::make_shared<std::vector<std::uint8_t>>(
            static_cast<std::uint8_t*>(buffers_.at(buffer.index).start),
            static_cast<std::uint8_t*>(buffers_.at(buffer.index).start) +
                buffer.bytesused);
    } catch (...) {
        xioctl(fd_, VIDIOC_QBUF, &buffer);
        throw;
    }

    if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
        throw_errno("VIDIOC_QBUF");
    }
    return frame;
}

}  // namespace vision
