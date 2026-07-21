#include "vision/inference.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace vision {
namespace {

std::uint64_t monotonic_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

InferenceResult NullInferenceBackend::run(const Frame& frame) {
    InferenceResult result;
    result.frame_id = frame.frame_id;
    result.captured_at_ns = frame.timestamp_ns;
    result.completed_at_ns = monotonic_ns();
    return result;
}

InferenceCoordinator::InferenceCoordinator(
    std::vector<std::unique_ptr<InferenceBackend>> backends)
    : backends_(std::move(backends)) {
    if (backends_.empty()) {
        throw std::invalid_argument("at least one inference backend is required");
    }
}

InferenceCoordinator::~InferenceCoordinator() {
    stop();
}

void InferenceCoordinator::start() {
    if (running_.exchange(true)) {
        return;
    }
    for (const auto& backend : backends_) {
        workers_.emplace_back(&InferenceCoordinator::worker_loop, this,
                              backend.get());
    }
}

void InferenceCoordinator::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    pending_cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void InferenceCoordinator::submit(Frame frame) {
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (!pending_frame_ || frame.frame_id > pending_frame_->frame_id) {
            pending_frame_ = std::move(frame);
        }
    }
    pending_cv_.notify_one();
}

std::optional<InferenceResult> InferenceCoordinator::consume_latest(
    std::uint64_t after_frame_id) {
    std::lock_guard<std::mutex> lock(result_mutex_);
    if (!latest_result_ || latest_result_->frame_id <= after_frame_id) {
        return std::nullopt;
    }
    return latest_result_;
}

std::uint64_t InferenceCoordinator::completed_count() const {
    return completed_.load();
}

bool InferenceCoordinator::pop_pending(Frame& frame) {
    std::unique_lock<std::mutex> lock(pending_mutex_);
    pending_cv_.wait(lock, [this] {
        return !running_.load() || pending_frame_.has_value();
    });
    if (!running_.load() && !pending_frame_) {
        return false;
    }
    frame = std::move(*pending_frame_);
    pending_frame_.reset();
    return true;
}

void InferenceCoordinator::publish(InferenceResult result) {
    completed_.fetch_add(1);
    std::lock_guard<std::mutex> lock(result_mutex_);
    if (!latest_result_ || result.frame_id > latest_result_->frame_id) {
        latest_result_ = std::move(result);
    }
}

void InferenceCoordinator::worker_loop(InferenceBackend* backend) {
    Frame frame;
    while (pop_pending(frame)) {
        publish(backend->run(frame));
    }
}

}  // namespace vision
