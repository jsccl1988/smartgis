// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/paint/paint_commit.h"

#include <atomic>
#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/raster/paint_stats.h"

namespace ui {
namespace views {
namespace {

std::atomic<std::uint64_t> g_commit_generation{1};

}  // namespace

bool commit_view_tree(View* root,
                      const Rect& dirty,
                      int width_px,
                      int height_px,
                      int font_px,
                      ui::gfx::Color clear_color,
                      PaintCommit* out) {
  if (!root || !out || width_px <= 0 || height_px <= 0) {
    return false;
  }

  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  out->display_list.clear();
  out->dirty = dirty;
  out->width_px = width_px;
  out->height_px = height_px;
  out->font_px = font_px > 0 ? font_px : 12;
  out->clear_color = clear_color;
  out->generation = g_commit_generation.fetch_add(1, std::memory_order_relaxed);

  // Recording stays on this (UI) thread only; thread_local recorder is not
  // shared with the compositor worker. Empty or full-client dirty skips
  // culling so product paths keep a complete tree snapshot.
  const Rect* cull = nullptr;
  if (!dirty.is_empty()) {
    const bool full_client = dirty.x <= 0 && dirty.y <= 0 &&
                             dirty.right() >= width_px &&
                             dirty.bottom() >= height_px;
    if (!full_client) {
      cull = &dirty;
    }
  }
  root->append_commands_to(&out->display_list, cull);

  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    const auto ticks =
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart);
    ui::gfx::note_commit_qpc(ticks);
    // Hover-sized dirty: track separately for U0/U1 shell hover gates.
    if (!dirty.is_empty()) {
      const std::int64_t area =
          static_cast<std::int64_t>(dirty.width) *
          static_cast<std::int64_t>(dirty.height);
      if (area > 0 && area < 64 * 64) {
        ui::gfx::note_hover_commit_qpc(ticks);
      }
    }
  }
  return true;
}

}  // namespace views
}  // namespace ui
