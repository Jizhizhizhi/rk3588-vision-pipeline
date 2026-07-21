#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "vision/inference.hpp"

namespace {

class DelayBackend final : public vision::InferenceBackend {
public:
    explicit DelayBackend(int delay_ms) : delay_ms_(delay_ms) {}

    vision::InferenceResult run(const vision::Frame& frame) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        vision::InferenceResult result;
        result.frame_id = frame.frame_id;
        result.captured_at_ns = frame.timestamp_ns;
        return result;
    }

private:
    int delay_ms_;
};

vision::Frame frame(std::uint64_t id) {
    vision::Frame value;
    value.frame_id = id;
    value.bytes = std::make_shared<std::vector<std::uint8_t>>();
    return value;
}

}  // namespace

int main() {
    std::vector<std::unique_ptr<vision::InferenceBackend>> backends;
    backends.emplace_back(std::make_unique<DelayBackend>(20));
    backends.emplace_back(std::make_unique<DelayBackend>(1));

    vision::InferenceCoordinator coordinator(std::move(backends));
    coordinator.start();
    coordinator.submit(frame(1));
    coordinator.submit(frame(2));
    coordinator.submit(frame(3));

    std::optional<vision::InferenceResult> latest;
    for (int i = 0; i < 100; ++i) {
        latest = coordinator.consume_latest(0);
        if (latest && latest->frame_id == 3) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    coordinator.stop();

    if (!latest || latest->frame_id != 3) {
        std::cerr << "latest-result delivery failed\n";
        return 1;
    }
    return 0;
}
