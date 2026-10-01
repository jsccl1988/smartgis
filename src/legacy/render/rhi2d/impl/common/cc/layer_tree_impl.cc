// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/cc/layer_tree_impl.h"

namespace render {
namespace detail {

bool Rhi2dLayerTreeImpl::activate(uint64_t generation,
                                  const SmtRenderContext& rc) {
  generation_ = generation;
  context_ = rc;
  active_ = true;
  return true;
}

void Rhi2dLayerTreeImpl::retire() { active_ = false; }

void Rhi2dLayerTreeImpl::set_damage(const RECT& damage, bool full_damage) {
  damage_ = damage;
  full_damage_ = full_damage;
}

void Rhi2dLayerTreeImpl::set_full_damage() {
  damage_ = RECT{0, 0, 0, 0};
  full_damage_ = true;
}

RECT Rhi2dLayerTreeImpl::resolved_damage(int viewport_w,
                                         int viewport_h) const {
  if (full_damage_ || viewport_w < 1 || viewport_h < 1) {
    RECT full{};
    full.left = 0;
    full.top = 0;
    full.right = viewport_w > 0 ? viewport_w : 0;
    full.bottom = viewport_h > 0 ? viewport_h : 0;
    return full;
  }
  RECT out = damage_;
  if (out.left < 0) {
    out.left = 0;
  }
  if (out.top < 0) {
    out.top = 0;
  }
  if (out.right > viewport_w) {
    out.right = viewport_w;
  }
  if (out.bottom > viewport_h) {
    out.bottom = viewport_h;
  }
  if (out.right < out.left) {
    out.right = out.left;
  }
  if (out.bottom < out.top) {
    out.bottom = out.top;
  }
  return out;
}

}  // namespace detail
}  // namespace render
