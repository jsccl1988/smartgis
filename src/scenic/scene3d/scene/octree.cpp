// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/scene/octree.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "scenic/render/rhi3d/impl/common/frame/prep_runner.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/scene/d3d_deferred_objects.h"

namespace scenic {
namespace detail {
namespace {

bool object_aabb_in_frustum(const Frustum& frustum, Object3d* obj) {
  if (!obj) {
    return false;
  }
  return frustum.intersects(obj->GetAabb());
}

}  // namespace

SceneOctree::SceneOctree() = default;

SceneOctree::~SceneOctree() { clear(); }

long SceneOctree::rebuild(Object3dPtrs& objects) {
  if (objects.size() < 1) {
    return kErrInvalidParam;
  }

  clear();
  objects_ = objects;
  all_targets_ = static_cast<int>(objects_.size());
  merge_aabb();
  return kErrNone;
}

long SceneOctree::clear() {
  objects_.clear();
  all_targets_ = 0;
  cur_targets_ = 0;
  aabb_ = Aabb();
  return kErrNone;
}

void SceneOctree::merge_aabb() {
  for (Object3d* obj : objects_) {
    if (obj) {
      aabb_.merge(obj->GetAabb());
    }
  }
  aabb_.vcCenter = (aabb_.vcMax + aabb_.vcMin) / 2.;
}

long SceneOctree::Update(LP3DRENDERDEVICE device, float elapsed) {
  BASE_TRACE_EVENT("octree.Update", "scene3d");
  cur_targets_ = 0;
  for (Object3d* obj : objects_) {
    if (obj) {
      obj->Update(device, elapsed);
      ++cur_targets_;
    }
  }
  return kErrNone;
}

long SceneOctree::Render(LP3DRENDERDEVICE device) {
  BASE_TRACE_EVENT("octree.Render", "scene3d");
  if (objects_.empty()) {
    return kErrNone;
  }

  cur_targets_ = 0;
  Frustum frustum;
  device->GetFrustum(frustum);

  if (show_node_box_) {
    GpuStateManager* stateManager = device->GetStateManager();
    stateManager->SetLight(false);
    stateManager->Set2DTextures(false);
    const float width = static_cast<float>(
        (std::max)(aabb_.vcMax.x - aabb_.vcMin.x,
                   (std::max)(aabb_.vcMax.y - aabb_.vcMin.y,
                              aabb_.vcMax.z - aabb_.vcMin.z)));
    device->DrawCube3D(aabb_.vcCenter, width, Color(0., 1., 0., 1.));
    stateManager->SetLight(true);
    stateManager->Set2DTextures(true);
  }

  const size_t n = objects_.size();
  std::vector<uint8_t> in_frustum(n, 0);
  {
    BASE_TRACE_EVENT("frustum_cull", "rhi3d.prep");
    Rhi3dPrepRunner& prep = rhi3d_shared_prep_runner();
    prep.ensure_workers(rhi3d_prep_worker_count());
    prep.run_jobs(n, [&](size_t i) {
      Object3d* obj = objects_[i];
      if (!obj || !obj->IsVisible()) {
        return;
      }
      if (object_aabb_in_frustum(frustum, obj)) {
        in_frustum[i] = 1;
      }
    });
  }

  std::vector<Object3d*> visible;
  visible.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    if (in_frustum[i]) {
      visible.push_back(objects_[i]);
    }
  }

  if (!render_objects_d3d_deferred(device, visible)) {
    for (Object3d* obj : visible) {
      obj->Render(device);
      ++cur_targets_;
    }
  } else {
    cur_targets_ = static_cast<int>(visible.size());
  }
  return kErrNone;
}

long SceneOctree::select_objects(Object3dPtrs& selected,
                                 LP3DRENDERDEVICE device,
                                 const lPoint& point) {
  if (objects_.empty()) {
    return kErrFailure;
  }

  cur_targets_ = 0;
  Frustum frustum;
  device->GetFrustum(frustum);

  for (Object3d* obj : objects_) {
    if (!obj) {
      continue;
    }
    if (!object_aabb_in_frustum(frustum, obj)) {
      continue;
    }
    if (obj->Select(device, point)) {
      selected.push_back(obj);
    }
  }

  return kErrFailure;
}

void SceneOctree::multiply_object_model_matrices(Matrix& transform) {
  for (Object3d* obj : objects_) {
    if (obj && obj->IsVisible()) {
      obj->ModelTransMatrixMultiply(transform);
    }
  }
}

void SceneOctree::multiply_object_world_matrices(Matrix& transform) {
  for (Object3d* obj : objects_) {
    if (obj && obj->IsVisible()) {
      obj->WorldTransMatrixMultiply(transform);
    }
  }
}

void SceneOctree::debug_string(char* buf, int buf_length) const {
  snprintf(buf, buf_length, "render target:%d/%d", cur_targets_, all_targets_);
}

}  // namespace detail
}  // namespace scenic
