// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/raster/paint_stats.h"

namespace ui {
namespace gfx {
namespace {

#if !defined(NDEBUG)
PaintCounters g_counters;
#endif

}  // namespace

PaintCounters paint_counters() {
#if !defined(NDEBUG)
  return g_counters;
#else
  return {};
#endif
}

void reset_paint_counters() {
#if !defined(NDEBUG)
  g_counters = {};
#endif
}

void note_widget_paint_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.widget_paint_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_map_paint_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.map_paint_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_commit_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.commit_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_raster_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.raster_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_present_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.present_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_begin_frame_qpc(std::uint64_t qpc_ticks) {
#if !defined(NDEBUG)
  g_counters.begin_frame_qpc = qpc_ticks;
  ++g_counters.begin_frame_count;
#else
  (void)qpc_ticks;
#endif
}

void note_begin_frame_to_present_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.begin_frame_to_present_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_begin_frame_to_shell_present_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.begin_frame_to_shell_present_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_hover_commit_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.hover_commit_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_table_scroll_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.table_scroll_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_overlay_copy_bytes(std::uint64_t bytes) {
#if !defined(NDEBUG)
  g_counters.overlay_copy_bytes += bytes;
#else
  (void)bytes;
#endif
}

void note_overlay_commit_qpc(std::uint64_t ticks) {
#if !defined(NDEBUG)
  g_counters.overlay_commit_qpc += ticks;
#else
  (void)ticks;
#endif
}

void note_commit() {
#if !defined(NDEBUG)
  ++g_counters.commit_count;
#endif
}

void note_activate() {
#if !defined(NDEBUG)
  ++g_counters.activate_count;
#endif
}

void note_paint_area(std::uint64_t pixels, std::uint64_t rcpaint) {
#if !defined(NDEBUG)
  g_counters.paint_pixels += pixels;
  g_counters.rcpaint_area += rcpaint;
#else
  (void)pixels;
  (void)rcpaint;
#endif
}

void note_layout() {
#if !defined(NDEBUG)
  ++g_counters.layout_count;
#endif
}

void note_create_font() {
#if !defined(NDEBUG)
  ++g_counters.create_font;
#endif
}

void note_utf8_conversion() {
#if !defined(NDEBUG)
  ++g_counters.utf8_conversions;
#endif
}

void note_canvas_ctor() {
#if !defined(NDEBUG)
  ++g_counters.canvas_ctors;
#endif
}

void note_create_brush() {
#if !defined(NDEBUG)
  ++g_counters.create_brush;
#endif
}

void note_measure_text() {
#if !defined(NDEBUG)
  ++g_counters.measure_text;
#endif
}

}  // namespace gfx
}  // namespace ui
