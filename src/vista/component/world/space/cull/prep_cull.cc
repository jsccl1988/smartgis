// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/space/cull/prep_cull.h"

#include <cstdlib>
#include <thread>
#include <unordered_set>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/process/switches.h"

namespace vista {
namespace detail {
namespace {

constexpr std::size_t kParallelPrepMinMeshes = 8;
// unibn rebuild + radius query is slower than linear AABB until the set is
// large. 64 matches the WorldPass frustum unit (scene_gpu_test kTotal).
constexpr std::size_t kOctreePrepMinMeshes = 64;

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

bool switch_truthy_opt_in(const char* name) {
  const char* e = base::switch_cstr(name);
  if (!e || !e[0]) {
    return false;
  }
  if (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' || e[0] == 'F') {
    return false;
  }
  return true;
}

}  // namespace

VISTA_EXPORT bool frustum_cull_enabled() {
  if (const char* e = base::switch_cstr("scene3d-no-cull")) {
    if (e[0] == '1' && e[1] == '\0') {
      return false;
    }
  }
  if (const char* on = base::switch_cstr("scene3d-frustum-cull")) {
    return on[0] == '1' && on[1] == '\0';
  }
  return false;
}

VISTA_EXPORT int prep_parallel_requested_workers() {
  if (!switch_truthy_opt_in("gpuscene-prep-parallel")) {
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

VISTA_EXPORT void prep_cull_meshes(const std::vector<MeshCullItem>& meshes,
                                   const FrustumPlanes* frustum,
                                   std::vector<uint8_t>* visible,
                                   AabbOctree* index, const float* view,
                                   const float* proj) {
  (void)index;
  if (!visible) {
    return;
  }
  visible->assign(meshes.size(), 1);
  const bool cull_active = frustum_cull_enabled() && frustum != nullptr;
  if (!cull_active) {
    return;
  }

  auto mark_linear = [&](std::size_t i) {
    const MeshCullItem& item = meshes[i];
    if (item.index_count == 0 || !item.has_buffers) {
      (*visible)[i] = 0;
      return;
    }
    (*visible)[i] =
        aabb_intersects_frustum(item.aabb_min_x, item.aabb_min_y,
                                item.aabb_min_z, item.aabb_max_x,
                                item.aabb_max_y, item.aabb_max_z, *frustum)
            ? 1
            : 0;
  };

  if (view && proj && meshes.size() >= kOctreePrepMinMeshes) {
    std::vector<AabbBox> boxes(meshes.size());
    for (std::size_t i = 0; i < meshes.size(); ++i) {
      const MeshCullItem& item = meshes[i];
      boxes[i].min_x = item.aabb_min_x;
      boxes[i].min_y = item.aabb_min_y;
      boxes[i].min_z = item.aabb_min_z;
      boxes[i].max_x = item.aabb_max_x;
      boxes[i].max_y = item.aabb_max_y;
      boxes[i].max_z = item.aabb_max_z;
    }
    AabbOctree tree;
    tree.rebuild(boxes.data(), boxes.size());
    std::vector<uint32_t> hits;
    if (tree.query_frustum(*frustum, view, proj, &hits)) {
      visible->assign(meshes.size(), 0);
      std::unordered_set<uint32_t> keep(hits.begin(), hits.end());
      for (std::size_t i = 0; i < meshes.size(); ++i) {
        const MeshCullItem& item = meshes[i];
        if (item.index_count == 0 || !item.has_buffers) {
          continue;
        }
        (*visible)[i] = keep.count(static_cast<uint32_t>(i)) ? 1 : 0;
      }
      return;
    }
  }

  const int workers = prep_parallel_effective_workers(true);
  if (workers <= 1 || meshes.size() < kParallelPrepMinMeshes) {
    for (std::size_t i = 0; i < meshes.size(); ++i) {
      mark_linear(i);
    }
    return;
  }

  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, std::size_t{0}, meshes.size(),
      [&](std::size_t i) { mark_linear(i); },
      /*grain=*/0, static_cast<std::size_t>(workers));
}

}  // namespace detail
}  // namespace vista
