// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/compositor/underlay/underlay_bridge.h"

#include "gpu/device/gpu_device_hub.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "vista/pass/world/opaque_effect.h"
#include "vista/pass/world/pass.h"

namespace gpu {
namespace detail {
namespace {

bool record_effects_only(render::rhi::Device* device,
                         const std::vector<render::graph::Effect*>& effects,
                         const render::rhi::CameraMatrices* camera,
                         uint32_t width_px, uint32_t height_px) {
  if (!device || width_px == 0 || height_px == 0) {
    return false;
  }
  render::graph::ViewInput in;
  in.width_px = width_px;
  in.height_px = height_px;
  in.camera = camera;
  in.effects = effects;

  // Prefer graph::present when the device has a windowed swapchain. For the
  // headless GPU-process path (no HWND), present() still records + execute;
  // FlyCube Present is a no-op without swapchain.
  return render::graph::present(device, in);
}

}  // namespace

bool record_underlay_effects(
    render::rhi::Device* device,
    const std::vector<render::graph::Effect*>& effects,
    const render::rhi::CameraMatrices* camera, uint32_t width_px,
    uint32_t height_px) {
  if (effects.empty()) {
    return true;
  }
  return record_effects_only(device, effects, camera, width_px, height_px);
}

bool record_gpu_scene_underlay(render::rhi::Device* device,
                               vista::WorldPass* scene,
                               const render::rhi::CameraMatrices* camera,
                               uint32_t width_px, uint32_t height_px,
                               std::vector<uint8_t>* out_bgra) {
  if (out_bgra) {
    out_bgra->clear();
  }
  if (!device || !scene || width_px == 0 || height_px == 0) {
    return false;
  }
  vista::OpaqueEffect opaque(scene);
  std::vector<render::graph::Effect*> effects = {&opaque};
  const bool ok =
      record_effects_only(device, effects, camera, width_px, height_px);
  // TODO(gpu-rhi): FlyCube offscreen RT readback into |out_bgra| so the
  // compositor can inject a kBgra underlay quad without CPU DEM demo.
  (void)out_bgra;
  return ok;
}

bool record_hub_underlay(AdapterId adapter, render::rhi::Device* device,
                         const render::rhi::CameraMatrices* camera,
                         uint32_t width_px, uint32_t height_px) {
  return record_underlay_effects(device, device_hub().underlay_effects(adapter),
                                 camera, width_px, height_px);
}

}  // namespace detail
}  // namespace gpu
