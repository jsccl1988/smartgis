// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_bake_cache.h"

#include "vista/terrain/dem/cache/io.h"

#include <atomic>
#include <cstdint>

namespace vista {
namespace {

std::atomic<int64_t> g_load_ms{0};
std::atomic<int64_t> g_tess_ms{0};
std::atomic<int64_t> g_hypso_ms{0};
std::atomic<int> g_load_hit{0};
std::atomic<int> g_hypso_hit{0};

}  // namespace

void dem_bake_cache_warmup() {
  detail::warmup_dem_bake_cache();
}

DemPhaseSample dem_last_phase_sample() {
  DemPhaseSample s;
  s.load_ms = g_load_ms.load(std::memory_order_relaxed);
  s.tess_ms = g_tess_ms.load(std::memory_order_relaxed);
  s.hypso_ms = g_hypso_ms.load(std::memory_order_relaxed);
  s.load_cache_hit = g_load_hit.load(std::memory_order_relaxed);
  s.hypso_cache_hit = g_hypso_hit.load(std::memory_order_relaxed);
  return s;
}

void reset_dem_phase_sample() {
  g_load_ms.store(0, std::memory_order_relaxed);
  g_tess_ms.store(0, std::memory_order_relaxed);
  g_hypso_ms.store(0, std::memory_order_relaxed);
  g_load_hit.store(0, std::memory_order_relaxed);
  g_hypso_hit.store(0, std::memory_order_relaxed);
}

void note_dem_phase_load(int64_t ms, bool cache_hit) {
  g_load_ms.fetch_add(ms, std::memory_order_relaxed);
  if (cache_hit) {
    g_load_hit.fetch_add(1, std::memory_order_relaxed);
  }
}

void note_dem_phase_tess(int64_t ms) {
  g_tess_ms.fetch_add(ms, std::memory_order_relaxed);
}

void note_dem_phase_hypso(int64_t ms, bool cache_hit) {
  g_hypso_ms.fetch_add(ms, std::memory_order_relaxed);
  if (cache_hit) {
    g_hypso_hit.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace vista
