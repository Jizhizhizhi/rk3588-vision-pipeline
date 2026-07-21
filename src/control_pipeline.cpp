#include "vision/control.hpp"

namespace vision {

std::optional<ControlCommand> NullControlPolicy::evaluate(
    const InferenceResult&) {
    return std::nullopt;
}

void NullControlSink::submit(const ControlCommand&) {}

ControlDispatcher::ControlDispatcher(ControlPolicy& policy, ControlSink& sink)
    : policy_(policy), sink_(sink) {}

void ControlDispatcher::dispatch(const InferenceResult& result) {
    if (result.frame_id <= last_frame_id_) {
        return;
    }
    last_frame_id_ = result.frame_id;
    if (auto command = policy_.evaluate(result)) {
        sink_.submit(*command);
    }
}

}  // namespace vision
