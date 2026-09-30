// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_LAYOUT_CHECK_H_
#define UI_VIEWS_KERNEL_LAYOUT_LAYOUT_CHECK_H_

#include "ui/ui_export.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Lightweight layout invariants (visual smoke). Used by
// views_unittests and SmartGisViews --self-test. Optional paint_fingerprint
// locks a fixed-size scene via GDI DIB hash (no external screenshot deps).

UI_EXPORT bool rect_non_negative(const Rect& r);
UI_EXPORT bool rect_contains_rect(const Rect& outer, const Rect& inner);

// True when |inner| center is within |tol_px| of |outer| center (view space).
UI_EXPORT bool rect_approximately_centered(const Rect& inner,
                                 const Rect& outer,
                                 int tol_px);

// Walk |root| (visible nodes). Appends human-readable violation codes:
//   "negative-bounds@…", "child-outside-parent@…", "zero-size-leaf@…"
// Returns the number of issues found.
UI_EXPORT int collect_layout_violations(const View* root, std::vector<std::string>* out);

// True when two axis-aligned rects share a positive-area intersection.
UI_EXPORT bool rects_overlap_positive(const Rect& a, const Rect& b);

// Walk |root|: for each parent, report pairs of locally-visible children whose
// bounds overlap with positive area (classic "stacked controls" bug).
// Codes: "sibling-overlap@parent>(ax,ay,aw,ah)x(bx,by,bw,bh)".
// Parents with allows_child_overflow() are skipped. Returns issue count.
UI_EXPORT int collect_sibling_overlaps(const View* root,
                                       std::vector<std::string>* out);

// Shell-specific invariants for Map HWND vs tab header, status bar inside
// root, and Catalog/Map tab-strip Y alignment. Appends codes:
//   "map-hwnd-covers-tabs@…", "status-outside-root@…",
//   "catalog-map-tab-y-skew@…", "inactive-map-hwnd-visible@…"
// |map_tabs| / |catalog_tabs| should be TabStrip*; |active_map| a MapViewport.
UI_EXPORT int collect_shell_layout_anomalies(
    const View* root,
    View* map_tabs,
    View* catalog_tabs,
    View* status_bar,
    View* active_map,
    View* inactive_map_a,
    View* inactive_map_b,
    std::vector<std::string>* out);

// Menu / tab shell: item height and horizontal gap at |scale|.
UI_EXPORT bool menu_item_metrics_ok(int item_width_px,
                          int item_height_px,
                          float scale);

// Paint |root| into a |width|×|height| 32-bpp DIB and return an FNV-1a hash of
// the pixels. Lays out |root| to fill the surface first. Returns 0 on failure.
UI_EXPORT std::uint32_t paint_fingerprint(View* root, int width, int height);

// Write violation lines to |path| (creates parent dirs). Returns false on I/O
// failure. Empty |issues| still writes a zero-byte-ok marker line "# clean".
UI_EXPORT bool write_layout_issues_file(const std::filesystem::path& path,
                                        const std::vector<std::string>& issues);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_LAYOUT_CHECK_H_
