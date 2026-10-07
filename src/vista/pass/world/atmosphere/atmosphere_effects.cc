// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/atmosphere_effects.h"

namespace vista {

PreOpaqueEffect::PreOpaqueEffect(AtmosphereFrame* frame) : frame_(frame) {}

render::graph::EffectSlot PreOpaqueEffect::slot() const {
  return render::graph::EffectSlot::kBeforeOpaque;
}

bool PreOpaqueEffect::clears_color() const {
  return frame_ && frame_->clears_color();
}

bool PreOpaqueEffect::uses_shared_depth() const {
  return frame_ && frame_->uses_shared_depth();
}

bool PreOpaqueEffect::record(const render::graph::RecordContext& ctx) {
  if (!frame_) {
    return false;
  }
  return frame_->record_pre_opaque(ctx.device, ctx.list, ctx.width, ctx.height,
                                   ctx.camera);
}

PostOpaqueEffect::PostOpaqueEffect(AtmosphereFrame* frame, int cloud_quality)
    : frame_(frame), cloud_quality_(cloud_quality) {}

render::graph::EffectSlot PostOpaqueEffect::slot() const {
  return render::graph::EffectSlot::kAfterOpaque;
}

bool PostOpaqueEffect::clears_color() const {
  return frame_ && frame_->clears_color();
}

bool PostOpaqueEffect::uses_shared_depth() const {
  return frame_ && frame_->uses_shared_depth();
}

bool PostOpaqueEffect::record(const render::graph::RecordContext& ctx) {
  if (!frame_) {
    return false;
  }
  return frame_->record_post_opaque(ctx.device, ctx.list, ctx.width, ctx.height,
                                    ctx.camera, cloud_quality_);
}

AtmosphereEffects::AtmosphereEffects(AtmosphereFrame* frame, int cloud_quality)
    : pre_(frame), post_(frame, cloud_quality) {}

render::graph::Effect* AtmosphereEffects::pre_effect() { return &pre_; }

render::graph::Effect* AtmosphereEffects::post_effect() { return &post_; }

void AtmosphereEffects::append_to(std::vector<render::graph::Effect*>* effects) {
  if (!effects) {
    return;
  }
  effects->push_back(&pre_);
  effects->push_back(&post_);
}

}  // namespace vista
