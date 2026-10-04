// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Caller-linked opaque-slot adapter. Binds the RecordContext camera once,
// then asks GpuScene to emit meshes. The frame graph does not include
// scene.h. Effect's vtable stays in frame_graph.cc inside render.dll.

#ifndef EFFECT_SCENE_OPAQUE_EFFECT_H_
#define EFFECT_SCENE_OPAQUE_EFFECT_H_

#include "render/graph/frame_graph.h"

#include "vista/vista_export.h"

namespace vista {

class GpuScene;

// Terrain, model, and map-world draws for kOpaque. The graph chooses load
// versus clear; this effect applies that choice and reports no color clear.
class VISTA_EXPORT OpaqueEffect final : public render::graph::Effect {
 public:
  explicit OpaqueEffect(GpuScene* scene);

  render::graph::EffectSlot slot() const override;
  bool clears_color() const override;
  bool uses_shared_depth() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  GpuScene* scene_ = nullptr;
};

}  // namespace vista

#endif  // EFFECT_SCENE_OPAQUE_EFFECT_H_
