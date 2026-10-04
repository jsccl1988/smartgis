// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scene3d_phase_profile.h"

#include "vista/world/terrain/dem/dem_bake_cache.h"

#include <atomic>
#include <mutex>

namespace content {
namespace {

std::atomic<int64_t> g_mesh_ms{0};
std::atomic<int64_t> g_sync_ms{0};
std::atomic<int64_t> g_rebuild_ms{0};
std::atomic<int64_t> g_ocean_prep_ms{0};
std::atomic<int64_t> g_record_ms{0};
std::atomic<int64_t> g_present_ms{0};
std::atomic<int64_t> g_upload_ms{0};
std::atomic<int64_t> g_pso_ms{0};
std::atomic<int> g_rebuild_count{0};

std::mutex g_cold_mu;
Scene3dColdPhaseSample g_cold;
bool g_cold_captured = false;

}  // namespace

Scene3dPhaseSample scene3d_last_phase_sample() {
  Scene3dPhaseSample s;
  s.mesh_ms = g_mesh_ms.load(std::memory_order_relaxed);
  s.sync_ms = g_sync_ms.load(std::memory_order_relaxed);
  s.rebuild_ms = g_rebuild_ms.load(std::memory_order_relaxed);
  s.ocean_prep_ms = g_ocean_prep_ms.load(std::memory_order_relaxed);
  s.record_ms = g_record_ms.load(std::memory_order_relaxed);
  s.present_ms = g_present_ms.load(std::memory_order_relaxed);
  s.upload_ms = g_upload_ms.load(std::memory_order_relaxed);
  s.pso_ms = g_pso_ms.load(std::memory_order_relaxed);
  s.rebuild_count = g_rebuild_count.load(std::memory_order_relaxed);
  return s;
}

Scene3dColdPhaseSample scene3d_cold_phase_sample() {
  std::lock_guard<std::mutex> lock(g_cold_mu);
  return g_cold;
}

void reset_scene3d_phase_sample() {
  g_mesh_ms.store(0, std::memory_order_relaxed);
  g_sync_ms.store(0, std::memory_order_relaxed);
  g_rebuild_ms.store(0, std::memory_order_relaxed);
  g_ocean_prep_ms.store(0, std::memory_order_relaxed);
  g_record_ms.store(0, std::memory_order_relaxed);
  g_present_ms.store(0, std::memory_order_relaxed);
  g_upload_ms.store(0, std::memory_order_relaxed);
  g_pso_ms.store(0, std::memory_order_relaxed);
  g_rebuild_count.store(0, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(g_cold_mu);
    g_cold = Scene3dColdPhaseSample{};
    g_cold_captured = false;
  }
  vista::reset_dem_phase_sample();
}

void scene3d_capture_cold_phase() {
  std::lock_guard<std::mutex> lock(g_cold_mu);
  if (g_cold_captured) {
    return;
  }
  const Scene3dPhaseSample phase = scene3d_last_phase_sample();
  const vista::DemPhaseSample dem = vista::dem_last_phase_sample();
  g_cold.dem_load_ms = dem.load_ms;
  g_cold.tess_ms = dem.tess_ms;
  g_cold.hypso_ms = dem.hypso_ms;
  g_cold.load_cache_hit = dem.load_cache_hit;
  g_cold.hypso_cache_hit = dem.hypso_cache_hit;
  g_cold.upload_ms = phase.upload_ms;
  g_cold.pso_ms = phase.pso_ms;
  // Exclusive record (exclude nested PSO + upload when timers overlap).
  int64_t exclusive_record = phase.record_ms - phase.upload_ms - phase.pso_ms;
  if (exclusive_record < 0) {
    exclusive_record = 0;
  }
  g_cold.record_ms = exclusive_record;
  g_cold.mesh_ms = phase.mesh_ms;
  g_cold.sync_ms = phase.sync_ms;
  g_cold.rebuild_ms = phase.rebuild_ms;
  g_cold.present_ms = phase.present_ms;
  g_cold_captured = true;
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

void note_scene3d_phase_upload(int64_t upload_ms) {
  g_upload_ms.store(upload_ms, std::memory_order_relaxed);
}

void note_scene3d_phase_pso(int64_t pso_ms) {
  g_pso_ms.store(pso_ms, std::memory_order_relaxed);
}

}  // namespace content
