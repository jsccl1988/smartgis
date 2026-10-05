// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/paint_schedule.h"

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace detail {

ui::gfx::Rect union_dirty_rects(const ui::gfx::Rect& a, const ui::gfx::Rect& b) {
  const bool a_ok = a.width > 0 && a.height > 0;
  const bool b_ok = b.width > 0 && b.height > 0;
  if (!a_ok) {
    return b_ok ? b : ui::gfx::Rect{};
  }
  if (!b_ok) {
    return a;
  }
  const int l = a.x < b.x ? a.x : b.x;
  const int t = a.y < b.y ? a.y : b.y;
  const int r = a.right() > b.right() ? a.right() : b.right();
  const int btm = a.bottom() > b.bottom() ? a.bottom() : b.bottom();
  return ui::gfx::Rect{l, t, r - l, btm - t};
}

bool is_large_dirty(const ui::gfx::Rect& pending,
                    bool full_paint,
                    int client_w,
                    int client_h) {
  if (full_paint) {
    return true;
  }
  if (pending.width <= 0 || pending.height <= 0 || client_w <= 0 ||
      client_h <= 0) {
    return false;
  }
  const std::int64_t dirty_area =
      static_cast<std::int64_t>(pending.width) *
      static_cast<std::int64_t>(pending.height);
  const std::int64_t client_area =
      static_cast<std::int64_t>(client_w) * static_cast<std::int64_t>(client_h);
  return dirty_area > client_area / 5;
}

std::int64_t refresh_frame_ticks() {
  LARGE_INTEGER freq = {};
  QueryPerformanceFrequency(&freq);
  if (freq.QuadPart <= 0) {
    return 0;
  }
  return freq.QuadPart / 60;
}

bool should_coalesce_commit(bool front_lags_client,
                            bool large_dirty,
                            std::int64_t now_qpc,
                            std::int64_t last_commit_qpc,
                            std::int64_t frame_ticks) {
  // large_dirty is informational for callers / tests; U1 must still coalesce
  // full-shell schedule_paint storms. Blocking on large_dirty forced a fresh
  // commit_view_tree on every WM_PAINT inside one refresh (+commit_ms). The
  // coalesce WM_TIMER still drains the unioned dirty. Resize stays gated by
  // front_lags_client (published front smaller than the client).
  (void)large_dirty;
  if (front_lags_client || frame_ticks <= 0 || last_commit_qpc == 0) {
    return false;
  }
  return (now_qpc - last_commit_qpc) < frame_ticks;
}

}  // namespace detail
}  // namespace views
}  // namespace ui
