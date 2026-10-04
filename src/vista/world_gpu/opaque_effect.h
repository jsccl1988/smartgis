// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Caller-linked opaque-slot adapter. Binds the RecordContext camera once,
// then asks WorldPass to emit meshes. The frame graph does not include
// pass.h. Effect's vtable stays in frame_graph.cc inside render.dll.

#ifndef VISTA_WORLD_GPU_OPAQUE_EFFECT_H_
#define VISTA_WORLD_GPU_OPAQUE_EFFECT_H_

#include "render/graph/frame_graph.h"

#include "vista/vista_export.h"

namespace vista {

class WorldPass;

// Terrain, model, and map-world draws for kOpaque. The graph chooses load
// versus clear; this effect applies that choice and reports no color clear.
class VISTA_EXPORT OpaqueEffect final : public render::graph::Effect {
 public:
  explicit OpaqueEffect(WorldPass* scene);

  render::graph::EffectSlot slot() const override;
  bool clears_color() const override;
  bool uses_shared_depth() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  WorldPass* scene_ = nullptr;
};

}  // namespace vista

#endif  // VISTA_WORLD_GPU_OPAQUE_EFFECT_H_
