// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_WIDGET_PAINT_SCHEDULE_H_
#define UI_VIEWS_KERNEL_WIDGET_PAINT_SCHEDULE_H_

#include "ui/ui_export.h"

#include <cstdint>

#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace views {
namespace detail {

// Union of two dirty rects. Empty (non-positive) sides are ignored.
UI_EXPORT ui::gfx::Rect union_dirty_rects(const ui::gfx::Rect& a,
                                          const ui::gfx::Rect& b);

// Scroll / resize: coalescing a large dirty presents a stale front.
UI_EXPORT bool is_large_dirty(const ui::gfx::Rect& pending,
                              bool full_paint,
                              int client_w,
                              int client_h);

// ~display refresh in QPC ticks (0 if the counter is unavailable).
UI_EXPORT std::int64_t refresh_frame_ticks();

// U5/U1: skip record when the previous Commit is still inside one refresh and
// the published front covers the client. |large_dirty| is retained for callers
// / tests but does not block coalesce (full-shell schedule_paint must not
// re-record every WM_PAINT inside ~16ms; the coalesce timer drains the union).
UI_EXPORT bool should_coalesce_commit(bool front_lags_client,
                                      bool large_dirty,
                                      std::int64_t now_qpc,
                                      std::int64_t last_commit_qpc,
                                      std::int64_t frame_ticks);

// WM_TIMER id: one delayed Invalidate after U5 coalesce so the last hover
// in a refresh window still Commits (immediate InvalidateRect would spin).
inline constexpr std::uintptr_t kShellCommitCoalesceTimer = 0x434F4C53u;

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_WIDGET_PAINT_SCHEDULE_H_
