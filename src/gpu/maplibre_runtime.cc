// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/maplibre_runtime.h"

// Optional MapLibre Native pin. Public GPU headers never include these.
#if defined(SMT_ENABLE_MAPLIBRE)
#if defined(__has_include)
#if __has_include(<mln/map.hpp>)
#include <mln/map.hpp>
#define SMT_HAS_MLN_MAP 1
#elif __has_include(<mbgl/map/map.hpp>)
#include <mbgl/map/map.hpp>
#define SMT_HAS_MBGL_MAP 1
#endif
#if __has_include(<mbgl/gfx/headless_frontend.hpp>)
#include <mbgl/gfx/headless_frontend.hpp>
#define SMT_HAS_MBGL_HEADLESS 1
#endif
#endif
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
#if defined(SMT_HAS_MLN_MAP) || defined(SMT_HAS_MBGL_MAP)
  return true;
#else
  return false;
#endif
}

bool try_maplibre_still_image(const char* style_json,
                              uint32_t width_px,
                              uint32_t height_px,
                              MaplibreStill* out) {
  (void)style_json;
  if (!out || width_px == 0 || height_px == 0) {
    return false;
  }
#if defined(SMT_HAS_MBGL_HEADLESS) && defined(SMT_HAS_MBGL_MAP)
  // Real mbgl::Map + HeadlessFrontend still-image path. Requires the
  // maplibre-native pin and a linked Windows lib — not present on this tree.
  return false;
#elif defined(SMT_HAS_MLN_MAP)
  // mln::Map exists as a header; this repo does not yet link a Windows
  // maplibre-native.lib, so a live still image cannot be produced here.
  return false;
#else
  return false;
#endif
}

}  // namespace detail
}  // namespace gpu
