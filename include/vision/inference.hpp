#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "vision/frame.hpp"

namespace vision {

struct Detection {
    int class_id = -1;
    float confidence = 0.0F;
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct InferenceResult {
    std::uint64_t frame_id = 0;
    std::uint64_t captured_at_ns = 0;
    std::uint64_t completed_at_ns = 0;
    std::vector<Detection> detections;
};

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;
    virtual InferenceResult run(const Frame& frame) = 0;
};

class NullInferenceBackend final : public InferenceBackend {
public:
    InferenceResult run(const Frame& frame) override;
};

class InferenceCoordinator {
public:
    InferenceCoordinator(std::vector<std::unique_ptr<InferenceBackend>> backends);
    ~InferenceCoordinator();

    InferenceCoordinator(const InferenceCoordinator&) = delete;
    InferenceCoordinator& operator=(const InferenceCoordinator&) = delete;

    void start();
    void stop();
    void submit(Frame frame);
    std::optional<InferenceResult> consume_latest(std::uint64_t after_frame_id);
    std::uint64_t completed_count() const;

private:
    void worker_loop(InferenceBackend* backend);
    bool pop_pending(Frame& frame);
    void publish(InferenceResult result);

    std::vector<std::unique_ptr<InferenceBackend>> backends_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> completed_{0};

    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::optional<Frame> pending_frame_;

    std::mutex result_mutex_;
    std::optional<InferenceResult> latest_result_;
};

}  // namespace vision
