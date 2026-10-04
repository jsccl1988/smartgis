// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/detail/prep_cull.h"

#include "vista/scene/detail/draw_pass.h"

#include <cstdlib>
#include <thread>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"

namespace vista {
namespace detail {
namespace {

// Skip pool spawn for tiny mesh lists (prep cost would dominate).
constexpr std::size_t kParallelPrepMinMeshes = 8;

int clamp_prep_workers() {
  const unsigned hc = std::thread::hardware_concurrency();
  if (hc <= 1) {
    return 1;
  }
  int n = static_cast<int>(hc / 2);
  if (n < 2) {
    n = 2;
  }
  if (n > 4) {
    n = 4;
  }
  return n;
}

bool env_truthy_opt_in(const char* e) {
  if (!e || !e[0]) {
    return false;
  }
  if (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
      e[0] == 'F') {
    return false;
  }
  return true;
}

}  // namespace

VISTA_EXPORT int prep_parallel_requested_workers() {
  // Product default stays serial until frustum cull + prep_cull_parallel are
  // both opted in. Unlike leftover SMT_RHI3D_PREP_PARALLEL (default on).
  const char* e = std::getenv("SMT_GPUSCENE_PREP_PARALLEL");
  if (!env_truthy_opt_in(e)) {
    return 1;
  }
  return clamp_prep_workers();
}

VISTA_EXPORT int prep_parallel_effective_workers(bool frustum_cull_active) {
  if (!frustum_cull_active) {
    return 1;
  }
  return prep_parallel_requested_workers();
}

VISTA_EXPORT void prep_cull_meshes(const std::vector<GpuScene::GpuMesh>& meshes,
                      const FrustumPlanes* frustum,
                      std::vector<uint8_t>* visible) {
  if (!visible) {
    return;
  }
  visible->assign(meshes.size(), 1);
  const bool cull_active = frustum_cull_enabled() && frustum != nullptr;
  if (!cull_active) {
    // Honesty gate: SMT_GPUSCENE_PREP_PARALLEL alone must not spawn workers
    // or do expensive AABB work when product frustum cull is still off.
    return;
  }

  auto mark = [&](std::size_t i) {
    const GpuScene::GpuMesh& mesh = meshes[i];
    if (mesh.index_count == 0 || !mesh.vertex || !mesh.index) {
      (*visible)[i] = 0;
      return;
    }
    (*visible)[i] = mesh_culled(mesh, frustum) ? 0 : 1;
  };

  const int workers = prep_parallel_effective_workers(true);
  if (workers <= 1 || meshes.size() < kParallelPrepMinMeshes) {
    for (std::size_t i = 0; i < meshes.size(); ++i) {
      mark(i);
    }
    return;
  }

  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, std::size_t{0}, meshes.size(),
      [&](std::size_t i) { mark(i); },
      /*grain=*/0, static_cast<std::size_t>(workers));
}

}  // namespace detail
}  // namespace vista
