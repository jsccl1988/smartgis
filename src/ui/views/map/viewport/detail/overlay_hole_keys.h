// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_DETAIL_OVERLAY_HOLE_KEYS_H_
#define UI_VIEWS_MAP_VIEWPORT_DETAIL_OVERLAY_HOLE_KEYS_H_

#include <cstdint>

#include "ui/gfx/color/color.h"
#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {
namespace detail {

// Named ARGB fills that must punch through staged shell overlay (alpha 0) so
// opaque horizon/panel bleeds do not src-over the GPU map. Byte-stable keys.
inline uint32_t overlay_hole_black() {
  return ui::gfx::color_rgb(0, 0, 0);
}
inline uint32_t overlay_hole_map_teal() {
  return ui::gfx::color_rgb(170, 211, 223);
}
inline uint32_t overlay_hole_navy() {
  return ui::gfx::color_rgb(18, 32, 48);
}

// Cap matches prior shell.cc stack array (caller + theme + named magics).
inline constexpr int kOverlayHoleKeyCap = 12;

// Pack unique 0x00RRGGBB keys from caller clears + Theme + named magics.
// Returns count written into |out_bgr| (capacity kOverlayHoleKeyCap).
inline int collect_overlay_hole_bgr_keys(uint32_t hole_clear_argb,
                                         uint32_t hole_clear_argb_alt,
                                         uint32_t* out_bgr) {
  if (!out_bgr) {
    return 0;
  }
  const Theme& theme = Theme::current();
  const uint32_t hole_argb[] = {
      hole_clear_argb,     hole_clear_argb_alt, theme.shell_bg,
      theme.panel_bg,      theme.panel_header,  theme.control_bg,
      theme.caption_bg,    theme.map_placeholder, overlay_hole_black(),
      overlay_hole_map_teal(), overlay_hole_navy(),
  };
  int hole_n = 0;
  for (uint32_t argb : hole_argb) {
    if (argb == 0) {
      continue;
    }
    const uint32_t key = argb & 0x00FFFFFFu;
    bool dup = false;
    for (int i = 0; i < hole_n; ++i) {
      if (out_bgr[i] == key) {
        dup = true;
        break;
      }
    }
    if (dup) {
      continue;
    }
    if (hole_n >= kOverlayHoleKeyCap) {
      break;
    }
    out_bgr[hole_n++] = key;
  }
  return hole_n;
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_DETAIL_OVERLAY_HOLE_KEYS_H_
