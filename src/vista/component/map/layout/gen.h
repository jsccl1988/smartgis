// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Host generation check shared by stage dispatch and painters.
// Painters must not include stage/emit.h just to read this.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_GEN_H_
#define VISTA_COMPONENT_MAP_LAYOUT_GEN_H_

#include <atomic>

#include "vista/component/map/layout.h"

namespace vista {
namespace detail {

// True when the host has advanced live_layout_gen past this build.
// Callers drop the rest of the tess; the host must not publish.
inline bool layout_gen_stale(const LayoutInput& in) {
  return in.live_layout_gen != nullptr && in.layout_gen != 0 &&
         in.live_layout_gen->load(std::memory_order_acquire) != in.layout_gen;
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_GEN_H_
