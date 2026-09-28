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
  // shared with the compositor worker.
  root->append_commands_to(&out->display_list);

  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_commit_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
  return true;
}

}  // namespace views
}  // namespace ui
