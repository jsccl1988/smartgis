// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/maplibre_runtime.h"

#if defined(SMT_ENABLE_MAPLIBRE) && defined(SMT_HAS_MAPLIBRE_LIB)
#include <mln/map.hpp>
#define SMT_MLN_MAP_LINKED 1
#endif

namespace gpu {
namespace detail {

bool maplibre_runtime_compiled_impl() {
#if defined(SMT_ENABLE_MAPLIBRE)
  return true;
#else
  return false;
#endif
}

bool maplibre_map_linked_impl() {
#if defined(SMT_MLN_MAP_LINKED)
  // Construct a live Map so the link is a lib, not a header probe.
  const mln::Map probe(1, 1);
  return probe.constructed();
#else
  return false;
#endif
}

bool try_maplibre_still_image(const char* style_json,
                              uint32_t width_px,
                              uint32_t height_px,
                              MaplibreStill* out) {
  if (!out || width_px == 0 || height_px == 0) {
    return false;
  }
#if defined(SMT_MLN_MAP_LINKED)
  mln::Map map(width_px, height_px);
  if (!map.constructed()) {
    return false;
  }
  map.load_style_json(style_json);
  out->width_px = width_px;
  out->height_px = height_px;
  out->bgra.assign(static_cast<size_t>(width_px) * height_px * 4u, 0);
  if (!map.render_still(out->bgra.data(), width_px * 4)) {
    out->bgra.clear();
    return false;
  }
  return true;
#else
  (void)style_json;
  return false;
#endif
}

}  // namespace detail
}  // namespace gpu
