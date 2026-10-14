// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_
#define UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_

#include <cstdlib>
#include <cstring>

#include "base/process/switches.h"

namespace ui {
namespace views {
namespace detail {

inline bool force_gdi_map_overlay() {
  return base::switch_is_one("force-gdi-map-overlay");
}

inline bool force_gdi_shell_overlay() {
  return base::switch_is_one("force-gdi-shell-overlay");
}

// CLI --map2d-engine=scenic or env MAP2D_ENGINE=scenic (same as former HUD).
inline bool prefer_map2d_scenic_engine() {
  const char* map_eng = base::switch_cstr("map2d-engine");
  if (map_eng && map_eng[0] && _stricmp(map_eng, "scenic") == 0) {
    return true;
  }
  if (const char* env = std::getenv("MAP2D_ENGINE")) {
    return env[0] != '\0' && _stricmp(env, "scenic") == 0;
  }
  return false;
}

inline bool force_content_mapview_2d() {
  return base::switch_is_one("force-content-mapview-2d");
}

// Default ON (legacy SmartGis yellow/black bar). map-identity-hud=0 hides.
inline bool show_map_identity_hud() {
  const char* v = base::switch_cstr("map-identity-hud");
  if (!v || v[0] == '\0') {
    return true;
  }
  return !(v[0] == '0' && v[1] == '\0');
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_
