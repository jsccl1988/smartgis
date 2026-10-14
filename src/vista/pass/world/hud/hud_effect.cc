// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/hud/hud_effect.h"

#include "vista/pass/world/hud/hud_pass.h"

namespace vista {

HudEffect::HudEffect(HudPass* pass) : pass_(pass) {}

render::graph::EffectSlot HudEffect::slot() const {
  return render::graph::EffectSlot::kOverlay;
}

bool HudEffect::record(const render::graph::RecordContext& ctx) {
  if (!pass_ || !ctx.device || !ctx.list) {
    return true;
  }
  return pass_->record(ctx.device, ctx.list, ctx.width, ctx.height);
}

}  // namespace vista
