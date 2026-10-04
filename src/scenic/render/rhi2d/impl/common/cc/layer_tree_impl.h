// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_COMMON_CC_LAYER_TREE_IMPL_H_
#define SCENIC_RHI2D_IMPL_COMMON_CC_LAYER_TREE_IMPL_H_

#include <cstdint>

#include <windows.h>

#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"

namespace scenic {
namespace detail {

// Active leftover map2d frame state after activate (cc::LayerTreeImpl analogue).
// Owns generation + damage only — GIS layers stay on Map / paint::Painter.
class Rhi2dLayerTreeImpl {
 public:
  Rhi2dLayerTreeImpl() = default;

  bool activate(uint64_t generation, const RenderContext& rc);
  void retire();

  bool is_active() const { return active_; }
  bool is_current(uint64_t generation) const {
    return active_ && generation_ == generation;
  }
  uint64_t generation() const { return generation_; }

  const RenderContext& context() const { return context_; }

  void set_damage(const RECT& damage, bool full_damage);
  void set_full_damage();
  const RECT& damage() const { return damage_; }
  bool full_damage() const { return full_damage_; }

  RECT resolved_damage(int viewport_w, int viewport_h) const;

 private:
  bool active_ = false;
  uint64_t generation_ = 0;
  RenderContext context_{};
  RECT damage_{0, 0, 0, 0};
  bool full_damage_ = true;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_COMMON_CC_LAYER_TREE_IMPL_H_
