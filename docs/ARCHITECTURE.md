# Architecture

## Capture

`V4L2Capture` opens a configured video node, requests MMAP buffers, queues them,
and copies each dequeued frame into an owned `Frame`. Copying is intentional in
this small reference implementation: ownership stays simple and a capture
buffer can be returned to the driver immediately.

Production systems may replace the copy with DMABUF import, but the lifetime
contract should remain explicit.

## Inference

`InferenceCoordinator` has a queue capacity of one. Submitting a newer frame
replaces the pending frame that no worker has started. Multiple workers may
complete out of order; `LatestResultSlot` keeps only the result with the
highest `frame_id`.

This prevents downstream control from chasing stale detections while still
allowing throughput statistics to count all completed worker jobs.

## Control Chain

`ControlPolicy` is intentionally an interface only. A caller may consume an
`InferenceResult` and optionally produce a `ControlCommand`. `ControlSink`
decides how, or whether, that command is applied.

No policy implementation, target ranking, prediction, feedback controller, or
input injection is part of this repository.

## HDMI Output

`DrmHdmiOutput` selects a connected DRM connector, its preferred mode, a usable
encoder/CRTC, and creates one dumb XRGB8888 framebuffer. `present()` copies a
full frame into the mapped buffer and programs the CRTC.

The implementation favors readability over zero-copy throughput. A production
path can replace the dumb buffer with GBM/DMABUF-backed buffers while retaining
the same output interface.
