#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "vision/capture.hpp"
#include "vision/control.hpp"
#include "vision/hdmi_output.hpp"
#include "vision/inference.hpp"

namespace {

std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

std::uint8_t clamp_byte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::vector<std::uint8_t> yuyv_to_xrgb(const vision::Frame& frame) {
    if (frame.format != vision::PixelFormat::kYuyv422 || !frame.bytes) {
        throw std::invalid_argument("expected an owned YUYV frame");
    }
    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(frame.width) * frame.height * 4);
    const auto& input = *frame.bytes;
    const std::size_t required =
        static_cast<std::size_t>(frame.stride) * frame.height;
    if (frame.stride < frame.width * 2 || input.size() < required) {
        throw std::invalid_argument("truncated YUYV frame");
    }
    for (std::uint32_t row = 0; row < frame.height; ++row) {
        const std::size_t input_row =
            static_cast<std::size_t>(row) * frame.stride;
        std::size_t out = static_cast<std::size_t>(row) * frame.width * 4;
        for (std::uint32_t x = 0; x + 1 < frame.width; x += 2) {
            const std::size_t i = input_row + static_cast<std::size_t>(x) * 2;
            const int y0 = static_cast<int>(input[i]) - 16;
            const int u = static_cast<int>(input[i + 1]) - 128;
            const int y1 = static_cast<int>(input[i + 2]) - 16;
            const int v = static_cast<int>(input[i + 3]) - 128;
            for (const int y : {y0, y1}) {
                const int c = std::max(0, y);
                const auto r = clamp_byte((298 * c + 409 * v + 128) >> 8);
                const auto g = clamp_byte(
                    (298 * c - 100 * u - 208 * v + 128) >> 8);
                const auto b = clamp_byte((298 * c + 516 * u + 128) >> 8);
                output[out++] = b;
                output[out++] = g;
                output[out++] = r;
                output[out++] = 0;
            }
        }
    }
    return output;
}

std::string option(int argc, char** argv, const std::string& name,
                   const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    const std::string capture_device = option(argc, argv, "--capture", "/dev/video0");
    const std::string drm_device = option(argc, argv, "--drm", "/dev/dri/card0");

    try {
        vision::DrmHdmiOutput output(drm_device);
        output.open();

        vision::V4L2Capture capture(capture_device, output.width(),
                                    output.height(), 60);
        capture.open();

        std::vector<std::unique_ptr<vision::InferenceBackend>> backends;
        backends.emplace_back(std::make_unique<vision::NullInferenceBackend>());
        vision::InferenceCoordinator inference(std::move(backends));
        inference.start();

        vision::NullControlPolicy policy;
        vision::NullControlSink sink;
        vision::ControlDispatcher control(policy, sink);
        std::uint64_t last_result_id = 0;

        while (running.load()) {
            auto frame = capture.read();
            inference.submit(frame);

            auto xrgb = yuyv_to_xrgb(frame);
            output.present_xrgb8888(
                xrgb.data(), xrgb.size(), frame.width, frame.height,
                frame.width * 4);

            if (auto result = inference.consume_latest(last_result_id)) {
                last_result_id = result->frame_id;
                control.dispatch(*result);
            }
        }

        inference.stop();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
