// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scene3d_phase_profile.h"

#include <atomic>

namespace content {
namespace {

std::atomic<int64_t> g_mesh_ms{0};
std::atomic<int64_t> g_sync_ms{0};
std::atomic<int64_t> g_rebuild_ms{0};
std::atomic<int64_t> g_ocean_prep_ms{0};
std::atomic<int64_t> g_record_ms{0};
std::atomic<int64_t> g_present_ms{0};
std::atomic<int> g_rebuild_count{0};

}  // namespace

Scene3dPhaseSample scene3d_last_phase_sample() {
  Scene3dPhaseSample s;
  s.mesh_ms = g_mesh_ms.load(std::memory_order_relaxed);
  s.sync_ms = g_sync_ms.load(std::memory_order_relaxed);
  s.rebuild_ms = g_rebuild_ms.load(std::memory_order_relaxed);
  s.ocean_prep_ms = g_ocean_prep_ms.load(std::memory_order_relaxed);
  s.record_ms = g_record_ms.load(std::memory_order_relaxed);
  s.present_ms = g_present_ms.load(std::memory_order_relaxed);
  s.rebuild_count = g_rebuild_count.load(std::memory_order_relaxed);
  return s;
}

void reset_scene3d_phase_sample() {
  g_mesh_ms.store(0, std::memory_order_relaxed);
  g_sync_ms.store(0, std::memory_order_relaxed);
  g_rebuild_ms.store(0, std::memory_order_relaxed);
  g_ocean_prep_ms.store(0, std::memory_order_relaxed);
  g_record_ms.store(0, std::memory_order_relaxed);
  g_present_ms.store(0, std::memory_order_relaxed);
  g_rebuild_count.store(0, std::memory_order_relaxed);
}

void note_scene3d_phase_mesh(int64_t mesh_ms) {
  g_mesh_ms.store(mesh_ms, std::memory_order_relaxed);
}

void note_scene3d_phase_sync(int64_t sync_ms) {
  g_sync_ms.store(sync_ms, std::memory_order_relaxed);
}

void note_scene3d_phase_rebuild(int64_t rebuild_ms, int count) {
  g_rebuild_ms.store(rebuild_ms, std::memory_order_relaxed);
  g_rebuild_count.store(count, std::memory_order_relaxed);
}

void note_scene3d_phase_ocean_prep(int64_t ocean_prep_ms) {
  g_ocean_prep_ms.store(ocean_prep_ms, std::memory_order_relaxed);
}

void note_scene3d_phase_record(int64_t record_ms) {
  g_record_ms.store(record_ms, std::memory_order_relaxed);
}

void note_scene3d_phase_present(int64_t present_ms) {
  g_present_ms.store(present_ms, std::memory_order_relaxed);
}

}  // namespace content
