// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_HUD_HUD_EFFECT_H_
#define VISTA_PASS_WORLD_HUD_HUD_EFFECT_H_

#include "render/graph/frame_graph.h"
#include "vista/vista_export.h"

namespace vista {

class HudPass;

// kOverlay adapter: screen HUD after opaque / atmosphere post.
class VISTA_EXPORT HudEffect final : public render::graph::Effect {
 public:
  explicit HudEffect(HudPass* pass);

  render::graph::EffectSlot slot() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  HudPass* pass_ = nullptr;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_HUD_HUD_EFFECT_H_
