// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_UNDERLAY_UNDERLAY_BRIDGE_H_
#define GPU_COMPOSITOR_UNDERLAY_UNDERLAY_BRIDGE_H_

#include "gpu/device/adapter_id.h"

#include <cstdint>
#include <vector>

namespace effect {
namespace scene {
class GpuScene;
}
}  // namespace effect

namespace render {
namespace rhi {
class Device;
struct CameraMatrices;
}  // namespace rhi
namespace graph {
class Effect;
}
}  // namespace render

namespace gpu {
namespace detail {

// Records Frame Graph / GpuScene underlay on the same AdapterId Device.
// Intentionally does NOT take OutputSurface — graph stays unaware of display.
//
// Pixel capture into |out_bgra| is best-effort. When false, the caller should
// keep the existing software/direct DEM underlay quads. Shell never includes
// this header.
bool record_underlay_effects(render::rhi::Device* device,
                             const std::vector<render::graph::Effect*>& effects,
                             const render::rhi::CameraMatrices* camera,
                             uint32_t width_px, uint32_t height_px);

// Wraps GpuScene as an OpaqueEffect and records it. |out_bgra| stays empty
// until RHI readback exists; returns true when recording succeeded.
bool record_gpu_scene_underlay(render::rhi::Device* device,
                               effect::scene::GpuScene* scene,
                               const render::rhi::CameraMatrices* camera,
                               uint32_t width_px, uint32_t height_px,
                               std::vector<uint8_t>* out_bgra);

// Hub convenience: record underlay Effects registered for |adapter|.
bool record_hub_underlay(AdapterId adapter, render::rhi::Device* device,
                         const render::rhi::CameraMatrices* camera,
                         uint32_t width_px, uint32_t height_px);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_UNDERLAY_UNDERLAY_BRIDGE_H_
