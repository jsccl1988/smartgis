// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <atomic>

namespace content {
namespace {

std::atomic<int64_t> g_layout_ms{0};
std::atomic<int64_t> g_hillshade_ms{0};
std::atomic<int64_t> g_software_paint_ms{0};
std::atomic<int64_t> g_bmp_io_ms{0};
std::atomic<int64_t> g_gpu_upload_ms{0};
std::atomic<int64_t> g_gpu_present_ms{0};

}  // namespace

Map2dPhaseSample map2d_last_phase_sample() {
  Map2dPhaseSample s;
  s.layout_ms = g_layout_ms.load(std::memory_order_relaxed);
  s.hillshade_ms = g_hillshade_ms.load(std::memory_order_relaxed);
  s.software_paint_ms = g_software_paint_ms.load(std::memory_order_relaxed);
  s.bmp_io_ms = g_bmp_io_ms.load(std::memory_order_relaxed);
  s.gpu_upload_ms = g_gpu_upload_ms.load(std::memory_order_relaxed);
  s.gpu_present_ms = g_gpu_present_ms.load(std::memory_order_relaxed);
  return s;
}

void reset_map2d_phase_sample() {
  g_layout_ms.store(0, std::memory_order_relaxed);
  g_hillshade_ms.store(0, std::memory_order_relaxed);
  g_software_paint_ms.store(0, std::memory_order_relaxed);
  g_bmp_io_ms.store(0, std::memory_order_relaxed);
  g_gpu_upload_ms.store(0, std::memory_order_relaxed);
  g_gpu_present_ms.store(0, std::memory_order_relaxed);
}

void note_map2d_phase_layout(int64_t layout_ms, int64_t hillshade_ms) {
  g_layout_ms.store(layout_ms, std::memory_order_relaxed);
  g_hillshade_ms.store(hillshade_ms, std::memory_order_relaxed);
}

void note_map2d_phase_software_paint(int64_t paint_ms) {
  g_software_paint_ms.store(paint_ms, std::memory_order_relaxed);
}

void note_map2d_phase_bmp_io(int64_t bmp_io_ms) {
  g_bmp_io_ms.store(bmp_io_ms, std::memory_order_relaxed);
}

void note_map2d_phase_gpu(int64_t upload_ms, int64_t present_ms) {
  g_gpu_upload_ms.store(upload_ms, std::memory_order_relaxed);
  g_gpu_present_ms.store(present_ms, std::memory_order_relaxed);
}

}  // namespace content
