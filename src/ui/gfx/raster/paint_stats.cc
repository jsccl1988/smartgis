// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/raster/paint_stats.h"

namespace ui {
namespace gfx {
namespace {

PaintCounters g_counters;

}  // namespace

PaintCounters paint_counters() {
  return g_counters;
}

void reset_paint_counters() {
  g_counters = {};
}

void note_widget_paint_qpc(std::uint64_t ticks) {
  g_counters.widget_paint_qpc += ticks;
}

void note_map_paint_qpc(std::uint64_t ticks) {
  g_counters.map_paint_qpc += ticks;
}

void note_commit_qpc(std::uint64_t ticks) {
  g_counters.commit_qpc += ticks;
}

void note_raster_qpc(std::uint64_t ticks) {
  g_counters.raster_qpc += ticks;
}

void note_present_qpc(std::uint64_t ticks) {
  g_counters.present_qpc += ticks;
}

void note_begin_frame_qpc(std::uint64_t qpc_ticks) {
  g_counters.begin_frame_qpc = qpc_ticks;
  ++g_counters.begin_frame_count;
}

void note_begin_frame_to_present_qpc(std::uint64_t ticks) {
  g_counters.begin_frame_to_present_qpc += ticks;
}

void note_begin_frame_to_shell_present_qpc(std::uint64_t ticks) {
  g_counters.begin_frame_to_shell_present_qpc += ticks;
}

void note_hover_commit_qpc(std::uint64_t ticks) {
  g_counters.hover_commit_qpc += ticks;
}

void note_table_scroll_qpc(std::uint64_t ticks) {
  g_counters.table_scroll_qpc += ticks;
}

void note_overlay_copy_bytes(std::uint64_t bytes) {
  g_counters.overlay_copy_bytes += bytes;
}

void note_overlay_commit_qpc(std::uint64_t ticks) {
  g_counters.overlay_commit_qpc += ticks;
}

void note_commit() {
  ++g_counters.commit_count;
}

void note_activate() {
  ++g_counters.activate_count;
}

void note_paint_area(std::uint64_t pixels, std::uint64_t rcpaint) {
  g_counters.paint_pixels += pixels;
  g_counters.rcpaint_area += rcpaint;
}

void note_layout() {
  ++g_counters.layout_count;
}

void note_create_font() {
  ++g_counters.create_font;
}

void note_utf8_conversion() {
  ++g_counters.utf8_conversions;
}

void note_canvas_ctor() {
  ++g_counters.canvas_ctors;
}

void note_create_brush() {
  ++g_counters.create_brush;
}

void note_measure_text() {
  ++g_counters.measure_text;
}

void note_paint_cache_hit() {
  ++g_counters.paint_cache_hit;
}

void note_paint_cache_miss() {
  ++g_counters.paint_cache_miss;
}

void note_layout_since_paint() {
  ++g_counters.layouts_since_paint;
}

void reset_layouts_since_paint() {
  g_counters.layouts_since_paint = 0;
}

void note_paint_lock_deferred() {
  ++g_counters.paint_lock_deferred;
}

}  // namespace gfx
}  // namespace ui
