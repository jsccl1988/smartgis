// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/scene/opaque_effect.h"

#include "effect/scene/scene.h"

namespace effect {
namespace scene {

OpaqueEffect::OpaqueEffect(GpuScene* scene) : scene_(scene) {}

render::graph::EffectSlot OpaqueEffect::slot() const {
  return render::graph::EffectSlot::kOpaque;
}

bool OpaqueEffect::clears_color() const { return false; }

bool OpaqueEffect::uses_shared_depth() const {
  // Depth chain is opened by PreOpaque when atmosphere is on; this effect only
  // consumes ctx.shared_depth. Returning true here (with atmosphere off) made
  // later passes assume a depth buffer that was never created.
  return false;
}

bool OpaqueEffect::record(const render::graph::RecordContext& ctx) {
  if (!scene_ || !ctx.device || !ctx.list) {
    return false;
  }
  scene_->set_color_load_op(ctx.color_op);
  scene_->set_enable_depth(ctx.shared_depth);
  // Always clear depth for opaque. Atmosphere pre-pass (ocean) writes the sea
  // plane into the shared DS; reloading that buffer makes low DEM samples fail
  // the depth test and paint a black mainland silhouette while land-only
  // showcases (no ocean) stay hypsometric-colored. Terrain then re-fills depth
  // for cloud/fog post passes.
  scene_->set_depth_load_op(render::rhi::DepthLoadOp::kClear);
  if (ctx.camera) {
    ctx.list->bind_camera(*ctx.camera);
  }
  return scene_->record_draws(ctx.device, ctx.list, ctx.width, ctx.height,
                              ctx.camera);
}

}  // namespace scene
}  // namespace effect
