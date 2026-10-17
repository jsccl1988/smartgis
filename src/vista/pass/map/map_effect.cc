// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/map_effect.h"

#include <utility>

#include "vista/pass/map/pass.h"

namespace vista {

MapEffect::MapEffect(
    render::graph::EffectSlot slot, MapPass* pass, const vista::MapIR* frame,
    const vista::View* view, GlyphRasterizer* glyphs,
    std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
                       int* h)>
        load_raster,
    std::function<bool(const std::string& symbol_id, std::vector<uint8_t>* rgba,
                       int* w, int* h)>
        load_icon,
    bool record_all, bool draw_overlays)
    : slot_(slot),
      pass_(pass),
      frame_(frame),
      view_(view),
      glyphs_(glyphs),
      load_raster_(std::move(load_raster)),
      load_icon_(std::move(load_icon)),
      record_all_(record_all),
      draw_overlays_(draw_overlays) {}

render::graph::EffectSlot MapEffect::slot() const {
  if (record_all_) {
    return render::graph::EffectSlot::kOpaque;
  }
  return slot_;
}

bool MapEffect::clears_color() const {
  // record_all and kOpaque world passes clear; kOverlay-only loads.
  return record_all_ || slot_ == render::graph::EffectSlot::kOpaque;
}

bool MapEffect::record(const render::graph::RecordContext& ctx) {
  if (!pass_ || !frame_ || !view_ || !ctx.device || !ctx.list) {
    return false;
  }
  const bool world = record_all_ || slot_ == render::graph::EffectSlot::kOpaque;
  // record_all rebuilds world meshes. Pixel labels are separate: a coverage
  // rebuild mid-drag still has record_all set, but must not draw river-label.
  const bool overlay = draw_overlays_ ||
                       slot_ == render::graph::EffectSlot::kOverlay;
  return pass_->record(ctx.device, ctx.list, *frame_, *view_, glyphs_,
                       load_raster_, load_icon_, ctx.camera, world, overlay,
                       ctx.color_op);
}

}  // namespace vista
