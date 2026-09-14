// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_LAYOUT_CHECK_H_
#define UI_VIEWS_LAYOUT_CHECK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ui {
namespace views {

// Lightweight layout invariants (Chromium-style visual smoke). Used by
// views_unittests and SmartGisViews --self-test. Optional paint_fingerprint
// locks a fixed-size scene via GDI DIB hash (no external screenshot deps).

bool rect_non_negative(const Rect& r);
bool rect_contains_rect(const Rect& outer, const Rect& inner);

// True when |inner| center is within |tol_px| of |outer| center (view space).
bool rect_approximately_centered(const Rect& inner,
                                 const Rect& outer,
                                 int tol_px);

// Walk |root| (visible nodes). Appends human-readable violation codes:
//   "negative-bounds@…", "child-outside-parent@…", "zero-size-leaf@…"
// Returns the number of issues found.
int collect_layout_violations(const View* root, std::vector<std::string>* out);

// Menu / tab chrome: item height and horizontal gap at |scale|.
bool menu_item_metrics_ok(int item_width_px,
                          int item_height_px,
                          float scale);

// Paint |root| into a |width|×|height| 32-bpp DIB and return an FNV-1a hash of
// the pixels. Lays out |root| to fill the surface first. Returns 0 on failure.
std::uint32_t paint_fingerprint(View* root, int width, int height);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_LAYOUT_CHECK_H_
