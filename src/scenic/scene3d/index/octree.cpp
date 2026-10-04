// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/index/octree.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Octree.hpp"
#include "base/trace/event/process_trace.h"
#include "scenic/render/rhi3d/impl/common/frame/prep_runner.h"
#include "scenic/render/rhi3d/public/device/base.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/detail/d3d_deferred_objects.h"

namespace scenic {
namespace detail {
namespace {

struct UnibnVec3 {
  float x;
  float y;
  float z;
};

bool object_aabb_in_frustum(const Frustum& frustum, Object3d* obj) {
  if (!obj) {
    return false;
  }
  return frustum.intersects(obj->GetAabb());
}

}  // namespace

// Frustum + unibn point index of object AABB centers (not exposed in header).
struct SceneOctreeAux {
  Frustum frustum;
  std::vector<UnibnVec3> points;
  unibn::Octree<UnibnVec3> tree;

  void clear_points() {
    points.clear();
    tree.clear();
  }

  void rebuild_from_objects(const Object3dPtrs& objects) {
    clear_points();
    points.reserve(objects.size());
    for (Object3d* obj : objects) {
      if (!obj) {
        continue;
      }
      const Vector3& c = obj->GetAabb().vcCenter;
      points.push_back(UnibnVec3{static_cast<float>(c.x),
                                 static_cast<float>(c.y),
                                 static_cast<float>(c.z)});
    }
    if (!points.empty()) {
      tree.initialize(points);
    }
  }
};

SceneOctTree::SceneOctTree()
    : m_aux(new SceneOctreeAux()),
      m_nAllRenderTargetsNum(0),
      m_nCurRenderTargets(0),
      m_bShowNodeBox(true) {}

SceneOctTree::~SceneOctTree() {
  DestroyTree();
  delete m_aux;
  m_aux = NULL;
}

long SceneOctTree::CreateOctTree(Object3dPtrs& v3DObjectPtrs) {
  if (v3DObjectPtrs.size() < 1) {
    return SMT_ERR_INVALID_PARAM;
  }

  DestroyTree();

  m_objects = v3DObjectPtrs;
  m_nAllRenderTargetsNum = static_cast<int>(m_objects.size());

  GetSceneDimensions(m_objects);

  if (m_aux) {
    m_aux->rebuild_from_objects(m_objects);
  }

  return SMT_ERR_NONE;
}

long SceneOctTree::DestroyTree() {
  m_objects.clear();
  m_nAllRenderTargetsNum = 0;
  m_nCurRenderTargets = 0;
  m_aabbScene = Aabb();
  if (m_aux) {
    m_aux->clear_points();
  }
  return SMT_ERR_NONE;
}

void SceneOctTree::GetSceneDimensions(Object3dPtrs& v3DObjectPtrs) {
  for (Object3d* obj : v3DObjectPtrs) {
    if (obj) {
      m_aabbScene.merge(obj->GetAabb());
    }
  }
  m_aabbScene.vcCenter = (m_aabbScene.vcMax + m_aabbScene.vcMin) / 2.;
}

long SceneOctTree::Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) {
  BASE_TRACE_EVENT("octree.Update", "scene3d");
  m_nCurRenderTargets = 0;
  // Device-touching Updates stay serial on the FrameJob thread.
  for (Object3d* obj : m_objects) {
    if (obj) {
      obj->Update(p3DRenderDevice, fElapsed);
      ++m_nCurRenderTargets;
    }
  }
  return SMT_ERR_NONE;
}

long SceneOctTree::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  BASE_TRACE_EVENT("octree.Render", "scene3d");
  if (!m_aux || m_objects.empty()) {
    return SMT_ERR_NONE;
  }

  m_nCurRenderTargets = 0;
  p3DRenderDevice->GetFrustum(m_aux->frustum);

  if (m_bShowNodeBox) {
    GpuStateManager* stateManager = p3DRenderDevice->GetStateManager();
    stateManager->SetLight(false);
    stateManager->Set2DTextures(false);
    const float width = static_cast<float>(
        (std::max)(m_aabbScene.vcMax.x - m_aabbScene.vcMin.x,
                   (std::max)(m_aabbScene.vcMax.y - m_aabbScene.vcMin.y,
                              m_aabbScene.vcMax.z - m_aabbScene.vcMin.z)));
    p3DRenderDevice->DrawCube3D(m_aabbScene.vcCenter, width,
                                Color(0., 1., 0., 1.));
    stateManager->SetLight(true);
    stateManager->Set2DTextures(true);
  }

  // CPU prep: parallel AABB-in-frustum. Workers never call the device.
  const size_t n = m_objects.size();
  std::vector<uint8_t> in_frustum(n, 0);
  {
    BASE_TRACE_EVENT("frustum_cull", "rhi3d.prep");
    detail::Rhi3dPrepRunner& prep = detail::rhi3d_shared_prep_runner();
    prep.ensure_workers(detail::rhi3d_prep_worker_count());
    Frustum& frustum = m_aux->frustum;
    const Object3dPtrs& objects = m_objects;
    prep.run_jobs(n, [&](size_t i) {
      Object3d* obj = objects[i];
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
    if (!in_frustum[i]) {
      continue;
    }
    visible.push_back(m_objects[i]);
  }

  if (!detail::render_objects_d3d_deferred(p3DRenderDevice, visible)) {
    for (Object3d* obj : visible) {
      obj->Render(p3DRenderDevice);
      ++m_nCurRenderTargets;
    }
  } else {
    m_nCurRenderTargets = static_cast<int>(visible.size());
  }
  return SMT_ERR_NONE;
}

long SceneOctTree::Select3DObject(Object3dPtrs& vSelected3DObjects,
                                     LP3DRENDERDEVICE p3DRenderDevice,
                                     const lPoint& point) {
  if (!m_aux || m_objects.empty()) {
    return SMT_ERR_FAILURE;
  }

  m_nCurRenderTargets = 0;
  p3DRenderDevice->GetFrustum(m_aux->frustum);

  for (Object3d* obj : m_objects) {
    if (!obj) {
      continue;
    }
    if (!object_aabb_in_frustum(m_aux->frustum, obj)) {
      continue;
    }
    if (obj->Select(p3DRenderDevice, point)) {
      vSelected3DObjects.push_back(obj);
    }
  }

  return SMT_ERR_FAILURE;
}

void SceneOctTree::ObjectModelMatrixMultiply(Matrix& matTransform) {
  for (Object3d* obj : m_objects) {
    if (obj && obj->IsVisible()) {
      obj->ModelTransMatrixMultiply(matTransform);
    }
  }
}

void SceneOctTree::ObjectWordlMatrixMultiply(Matrix& matTransform) {
  for (Object3d* obj : m_objects) {
    if (obj && obj->IsVisible()) {
      obj->WorldTransMatrixMultiply(matTransform);
    }
  }
}

void SceneOctTree::GetDebugString(char* szBuf, int nBufLength) {
  const size_t indexed = m_aux ? m_aux->points.size() : 0;
  snprintf(szBuf, nBufLength, "render target:%d/%d;unibn points:%zu",
           m_nCurRenderTargets, m_nAllRenderTargetsNum, indexed);
}

}  // namespace detail
}  // namespace scenic
