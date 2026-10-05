// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_
#define UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_

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

inline bool prefer_map2d_scenic_engine() {
  const char* map_eng = base::switch_cstr("map2d-engine");
  return map_eng && map_eng[0] && _stricmp(map_eng, "scenic") == 0;
}

inline bool force_content_mapview_2d() {
  return base::switch_is_one("force-content-mapview-2d");
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_PAINT_POLICY_H_
