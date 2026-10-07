// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Adapts AtmosphereFrame into two Effect slots. Does not own the frame.
// Other domains add their own Effect types; they do not subclass this pair.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_ATMOSPHERE_EFFECTS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_ATMOSPHERE_EFFECTS_H_

#include <vector>

#include "vista/pass/world/atmosphere/atmosphere_frame.h"
#include "render/graph/frame_graph.h"

#include "vista/vista_export.h"

namespace vista {

// Sky and ocean, before opaque geometry.
class PreOpaqueEffect final : public render::graph::Effect {
 public:
  explicit PreOpaqueEffect(AtmosphereFrame* frame);

  render::graph::EffectSlot slot() const override;
  bool clears_color() const override;
  bool uses_shared_depth() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  AtmosphereFrame* frame_ = nullptr;
};

// Cloud and fog, after opaque geometry. Captures cloud quality for the frame.
class PostOpaqueEffect final : public render::graph::Effect {
 public:
  PostOpaqueEffect(AtmosphereFrame* frame, int cloud_quality);

  render::graph::EffectSlot slot() const override;
  bool clears_color() const override;
  bool uses_shared_depth() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  AtmosphereFrame* frame_ = nullptr;
  int cloud_quality_ = 1;
};

// Caller-owned pair. pre_effect is the before-opaque slot; post_effect is
// the after-opaque slot. append_to pushes those two, pre then post.
class VISTA_EXPORT AtmosphereEffects {
 public:
  AtmosphereEffects(AtmosphereFrame* frame, int cloud_quality);

  render::graph::Effect* pre_effect();
  render::graph::Effect* post_effect();
  void append_to(std::vector<render::graph::Effect*>* effects);

 private:
  PreOpaqueEffect pre_;
  PostOpaqueEffect post_;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_ATMOSPHERE_EFFECTS_H_
