#pragma once

#include <cstdint>
#include <optional>

#include "vision/inference.hpp"

namespace vision {

struct ControlCommand {
    float delta_x = 0.0F;
    float delta_y = 0.0F;
    bool trigger = false;
};

class ControlPolicy {
public:
    virtual ~ControlPolicy() = default;
    virtual std::optional<ControlCommand> evaluate(
        const InferenceResult& result) = 0;
};

class ControlSink {
public:
    virtual ~ControlSink() = default;
    virtual void submit(const ControlCommand& command) = 0;
};

class NullControlPolicy final : public ControlPolicy {
public:
    std::optional<ControlCommand> evaluate(
        const InferenceResult& result) override;
};

class NullControlSink final : public ControlSink {
public:
    void submit(const ControlCommand& command) override;
};

class ControlDispatcher {
public:
    ControlDispatcher(ControlPolicy& policy, ControlSink& sink);
    void dispatch(const InferenceResult& result);

private:
    ControlPolicy& policy_;
    ControlSink& sink_;
    std::uint64_t last_frame_id_ = 0;
};

}  // namespace vision
