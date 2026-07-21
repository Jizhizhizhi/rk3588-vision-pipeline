# RKNN Backend Boundary

The repository intentionally does not redistribute model weights, Rockchip SDK
binaries, generated tensor metadata, or project-specific post-processing.

An RKNN adapter should implement `vision::InferenceBackend` and keep these
steps inside the adapter:

1. Validate the input frame dimensions and pixel format.
2. Convert or import the frame into the model input tensor.
3. Submit inference to one RKNN context owned by that worker instance.
4. Read output tensors.
5. Decode model-specific outputs into the portable `Detection` structure.
6. Release runtime-owned output buffers before returning.

Each worker must own its own runtime context unless the SDK explicitly
documents concurrent use of one context. The shared coordinator handles only
frame scheduling and result freshness.

```cpp
class RknnBackend final : public vision::InferenceBackend {
public:
    explicit RknnBackend(const std::string& model_path);
    vision::InferenceResult run(const vision::Frame& frame) override;

private:
    // One runtime context and its tensor metadata belong to this instance.
};
```

Do not append completed results to an ordered delivery queue. Workers may
finish out of order; downstream control should receive only the completed
result with the highest `frame_id`.
