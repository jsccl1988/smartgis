// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_RASTER_PAINT_STATS_H_
#define UI_GFX_RASTER_PAINT_STATS_H_

#include "ui/ui_export.h"
#include <cstdint>

namespace ui {
namespace gfx {

// Debug-only paint counters. Not a metrics framework: Release builds keep the
// functions but leave every field at zero and do not log.
struct PaintCounters {
  std::uint64_t widget_paint_qpc = 0;
  std::uint64_t map_paint_qpc = 0;
  // Shell compositor stages (UI commit / worker raster / UI present).
  std::uint64_t commit_qpc = 0;
  std::uint64_t raster_qpc = 0;
  std::uint64_t present_qpc = 0;
  // BeginFrame (DWM/vblank) → present latency gate (Debug accumulates).
  std::uint64_t begin_frame_qpc = 0;
  std::uint64_t begin_frame_to_present_qpc = 0;
  // Shell-only BeginFrame → shell present (U5); separate from map present.
  std::uint64_t begin_frame_to_shell_present_qpc = 0;
  std::uint64_t begin_frame_count = 0;
  // Scenario gates for shell perf waves (hover / table scroll / map overlay).
  std::uint64_t hover_commit_qpc = 0;
  std::uint64_t table_scroll_qpc = 0;
  std::uint64_t overlay_copy_bytes = 0;
  std::uint64_t overlay_commit_qpc = 0;
  std::uint64_t commit_count = 0;
  std::uint64_t activate_count = 0;
  std::uint64_t paint_pixels = 0;
  std::uint64_t rcpaint_area = 0;
  std::uint64_t layout_count = 0;
  std::uint64_t create_font = 0;
  std::uint64_t utf8_conversions = 0;
  std::uint64_t canvas_ctors = 0;
  std::uint64_t create_brush = 0;
  std::uint64_t measure_text = 0;
};

UI_EXPORT PaintCounters paint_counters();
UI_EXPORT void reset_paint_counters();

UI_EXPORT void note_widget_paint_qpc(std::uint64_t ticks);
UI_EXPORT void note_map_paint_qpc(std::uint64_t ticks);
UI_EXPORT void note_commit_qpc(std::uint64_t ticks);
UI_EXPORT void note_raster_qpc(std::uint64_t ticks);
UI_EXPORT void note_present_qpc(std::uint64_t ticks);
// Records a BeginFrame tick (QPC timestamp) and optional present latency.
UI_EXPORT void note_begin_frame_qpc(std::uint64_t qpc_ticks);
UI_EXPORT void note_begin_frame_to_present_qpc(std::uint64_t ticks);
UI_EXPORT void note_begin_frame_to_shell_present_qpc(std::uint64_t ticks);
UI_EXPORT void note_hover_commit_qpc(std::uint64_t ticks);
UI_EXPORT void note_table_scroll_qpc(std::uint64_t ticks);
UI_EXPORT void note_overlay_copy_bytes(std::uint64_t bytes);
UI_EXPORT void note_overlay_commit_qpc(std::uint64_t ticks);
UI_EXPORT void note_commit();
UI_EXPORT void note_activate();
UI_EXPORT void note_paint_area(std::uint64_t pixels, std::uint64_t rcpaint);
UI_EXPORT void note_layout();
UI_EXPORT void note_create_font();
UI_EXPORT void note_utf8_conversion();
UI_EXPORT void note_canvas_ctor();
UI_EXPORT void note_create_brush();
UI_EXPORT void note_measure_text();

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_RASTER_PAINT_STATS_H_
