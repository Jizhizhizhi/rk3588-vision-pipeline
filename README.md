# RK3588 Vision Pipeline

A backend-only Linux reference pipeline for V4L2 capture, latest-result-first
inference scheduling, pluggable target-control dispatch, and DRM/KMS HDMI
output. The repository is intentionally independent from any web frontend,
license service, model file, private device configuration, or deployment image.

## Included

- V4L2 MMAP capture with a small RAII wrapper.
- A bounded, latest-frame inference queue.
- Multi-worker inference scheduling that publishes only the newest completed
  result to downstream consumers.
- A target-control interface and dispatcher. No targeting, tracking, PID,
  prediction, or mouse-control algorithm is provided.
- DRM/KMS HDMI output using a dumb XRGB8888 framebuffer.
- High-level EDID design notes. No platform-specific EDID application source,
  device discovery logic, or capture-device information is included.

## Deliberately Excluded

- Web UI and HTTP APIs.
- Target-selection and motion-control algorithms.
- Model weights and proprietary SDK binaries.
- Activation, cloud-control, telemetry, fingerprinting, or update services.
- USB mouse passthrough and input injection.
- Production EDID synthesis/application code.

## Build

On Debian or Ubuntu:

```bash
sudo apt install build-essential cmake pkg-config libdrm-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The demo uses a null inference backend. Integrate an RKNN backend by
implementing `vision::InferenceBackend`; the scheduler and downstream
latest-result semantics do not depend on a specific model runtime.

```bash
sudo ./build/vision_pipeline_demo --capture /dev/video0 --drm /dev/dri/card0
```

The capture device must expose YUYV frames matching the selected mode. DRM/KMS
access usually requires root or suitable group/udev permissions.

## Data Flow

```text
V4L2 capture
    | newest pending frame only
    v
InferenceCoordinator (N workers)
    | newest completed frame_id only
    +----> ControlDispatcher -> user supplied policy/sink
    |
    +----> preview/output path -> YUYV to XRGB8888 -> DRM/KMS HDMI
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/EDID_DESIGN.md](docs/EDID_DESIGN.md). The expected RKNN adapter boundary
is described in [docs/RKNN_BACKEND.md](docs/RKNN_BACKEND.md).

## Responsible Use

This project is a systems-programming reference for local, authorized vision
experiments. Users are responsible for complying with applicable laws,
platform rules, device warranties, and third-party terms.

## License

MIT.
