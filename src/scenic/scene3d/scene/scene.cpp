// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/scene/scene.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/core/log.h"
#include "base/threading/thread.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "base/math/math.h"
#include "scenic/render/frame.h"
#include "scenic/render/rhi3d/impl/common/frame/prep_runner.h"
#include "scenic/scene3d/scene/d3d_deferred_objects.h"
#include "scenic/scene3d/scene/octree.h"
#include "scenic/scene3d/scene/scene_to_world.h"

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
Scene::Scene(void)
    : m_p3DRenderDevice(NULL),
      m_pTimer(NULL),
      m_pCamera(NULL),
      m_bOctTreeCreated(false),
      m_bShowNodeBox(true),
      m_nHelpInfoFont(0),
      m_nTimerInfoFont(0),
      m_nRenderInfoFont(0),
      m_pNorthArray(NULL),
      m_pSceneTree(NULL) {
  m_szHelpInfoBuf[0] = '\0';
  m_szRenderInfoBuf[0] = '\0';
}

Scene::~Scene(void) {
  Object3dPtrs::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    SAFE_DELETE(*iter);
    ++iter;
  }

  SAFE_DELETE(m_pNorthArray);
  SAFE_DELETE(m_pSceneTree);
  SAFE_DELETE(m_pTimer);

  m_pCamera = NULL;
  m_p3DRenderDevice = NULL;
}

inline void Scene::SetSceneCamera(PerspCamera *pCamera) {
  m_pCamera = pCamera;
  if (m_pNorthArray) m_pNorthArray->set_camera(m_pCamera);
}

long Scene::Setup() {
  m_pSceneTree = new SceneOctree();
  m_pTimer = new ::base::FrameTimer();
  m_pNorthArray = new NorthArray(90, 120, nullptr);
  Vector3 north_origin(0, 0, 0);
  Material north_mat;
  if (m_pNorthArray->Init(north_origin, north_mat) == kErrNone) {
    m_pNorthArray->Create(m_p3DRenderDevice);
  }

  m_pTimer->set_clock(0, 0);

  m_pSceneTree->set_show_node_box(m_bShowNodeBox);

  m_p3DRenderDevice->CreateFont("Calibri", 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                16, m_nRenderInfoFont);
  m_p3DRenderDevice->CreateFont("MS Reference Sans Serif", 16, 0, FW_BOLD,
                                FALSE, FALSE, FALSE, 16, m_nTimerInfoFont);
  m_p3DRenderDevice->CreateFont("Times New Roman", 16, 0, FW_BOLD, FALSE, FALSE,
                                FALSE, 10, m_nHelpInfoFont);

  LOGGING(LOG_INFO, "Scene::Setup() is ok!");

  return kErrNone;
}

long Scene::Update() {
  BASE_TRACE_EVENT("Update", "scene3d");
  if (m_pTimer && m_pCamera && m_pSceneTree) {
    m_pTimer->update();

    sprintf(m_szHelpInfoBuf,
            "Move:Front:W  Left:A  Back:S  Right:D Eye : X%f  Y:%f   Z:%f ",
            m_pCamera->eye().x, m_pCamera->eye().y,
            m_pCamera->eye().z);

    if (m_bOctTreeCreated) {
      m_pSceneTree->set_show_node_box(m_bShowNodeBox);

      m_pSceneTree->Update(m_p3DRenderDevice, m_pTimer->get_elapsed());

    } else {
      Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
      while (iter != m_v3DObjectPtrs.end()) {
        if (NULL != (*iter)) {
          (*iter)->Update(m_p3DRenderDevice, m_pTimer->get_elapsed());
        }
        iter++;
      }
    }

    if (m_pNorthArray)
      m_pNorthArray->Update(m_p3DRenderDevice, m_pTimer->get_elapsed());
  }

  return kErrNone;
}

long Scene::Render(void) {
  BASE_TRACE_EVENT("Render", "scene3d");
  detail::log_frame_flow("scene3d.Render");
  if (NULL != m_pSceneTree && NULL != m_p3DRenderDevice && m_pTimer != NULL) {
    if (m_pCamera) m_pCamera->apply();

    // scene object
    if (m_bOctTreeCreated) {
      static char szBuf[TEMP_BUFFER_SIZE];

      if (m_pSceneTree->IsVisible()) {
        m_pSceneTree->Render(m_p3DRenderDevice);
        m_pSceneTree->debug_string(szBuf, TEMP_BUFFER_SIZE);
        sprintf(m_szRenderInfoBuf, "Fps%.3f\t%s", m_pTimer->get_fps(), szBuf);
      } else
        sprintf(m_szRenderInfoBuf, "Fps%.3f\t", m_pTimer->get_fps());
    } else {
      // Integer FPS — D3D DrawText GDI path caches by string; fractional FPS
      // created a unique texture every Present.
      sprintf(m_szRenderInfoBuf, "Fps%d\t",
              static_cast<int>(m_pTimer->get_fps() + 0.5f));

      Frustum frustum;
      m_p3DRenderDevice->GetFrustum(frustum);
      const size_t n = m_v3DObjectPtrs.size();
      std::vector<uint8_t> in_frustum(n, 0);
      // SMT_RHI3D_SKIP_FRUSTUM=1: draw all visible objects (debug / D3D frustum
      // extract regressions).
      const bool skip_frustum = []() {
        const char* e = base::switch_cstr("rhi3d-skip-frustum");
        return e && e[0] && e[0] != '0' && e[0] != 'n' && e[0] != 'N';
      }();
      {
        BASE_TRACE_EVENT("frustum_cull", "rhi3d.prep");
        detail::Rhi3dPrepRunner& prep = detail::rhi3d_shared_prep_runner();
        prep.ensure_workers(detail::rhi3d_prep_worker_count());
        const Object3dPtrs& objects = m_v3DObjectPtrs;
        prep.run_jobs(n, [&](size_t i) {
          Object3d* obj = objects[i];
          if (!obj || !obj->IsVisible()) {
            return;
          }
          if (skip_frustum || object_aabb_in_frustum(frustum, obj)) {
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
        visible.push_back(m_v3DObjectPtrs[i]);
      }
      LARGE_INTEGER qpf = {};
      LARGE_INTEGER t_draw0 = {};
      LARGE_INTEGER t_draw1 = {};
      const bool time_draw = []() {
        const char* e = base::switch_cstr("rhi3d-time-present");
        return e && e[0] == '1';
      }();
      if (time_draw) {
        QueryPerformanceFrequency(&qpf);
        QueryPerformanceCounter(&t_draw0);
      }
      std::vector<Object3d*> immediate_after;
      if (!detail::render_objects_d3d_deferred(m_p3DRenderDevice, visible,
                                               &immediate_after)) {
        for (Object3d* obj : visible) {
          obj->Render(m_p3DRenderDevice);
        }
      } else if (!immediate_after.empty()) {
        // Restore camera matrices before screen-space labels (P3 may race MV).
        if (m_pCamera) {
          m_pCamera->apply();
        }
        for (Object3d* obj : immediate_after) {
          obj->Render(m_p3DRenderDevice);
        }
      }
      if (time_draw) {
        QueryPerformanceCounter(&t_draw1);
        static int s_draw_prints = 0;
        if (s_draw_prints < 5 && qpf.QuadPart > 0) {
          const double ms = 1000.0 *
                            static_cast<double>(t_draw1.QuadPart - t_draw0.QuadPart) /
                            static_cast<double>(qpf.QuadPart);
          std::fprintf(stderr,
                       "rhi3d.scene: objects=%zu visible=%zu draw=%.2f ms\n", n,
                       visible.size(), ms);
          std::fflush(stderr);
          ++s_draw_prints;
        }
      }
    }

    if (m_pNorthArray && m_pNorthArray->IsVisible())
      m_pNorthArray->Render(m_p3DRenderDevice);

    // Debug HUD (D3D GDI→sprite is costly; skip unless explicitly enabled).
    const bool draw_debug_hud = []() {
      const char* e = base::switch_cstr("rhi3d-debug-hud");
      if (!e || !e[0]) {
        return false;
      }
      return !(e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
               e[0] == 'F');
    }();
    if (draw_debug_hud) {
      // 3d text
      m_p3DRenderDevice->DrawText(m_nTimerInfoFont, 0, 0, 0, Color(0., 1., 0.),
                                  "(0,0,0)");

      // 2d text
      m_p3DRenderDevice->DrawText(m_nHelpInfoFont, 10, 24, Color(0., 0., 1.),
                                  m_szHelpInfoBuf);
      m_p3DRenderDevice->DrawText(m_nRenderInfoFont, 10, 44, Color(0., 1., 0.),
                                  m_szRenderInfoBuf);
      m_p3DRenderDevice->DrawText(m_nTimerInfoFont, 10, 64, Color(1., 1., 0.),
                                  m_pTimer->get_clock());
    }
  }

  return kErrNone;
}
long Scene::Transform2DTo3D(::base::Vector3 &vOrg, ::base::Vector3 &vTar,
                               const lPoint &point) {
  if (m_pCamera) m_pCamera->apply();

  if (NULL != m_p3DRenderDevice)
    m_p3DRenderDevice->Transform2DTo3D(vOrg, vTar, point);

  return kErrNone;
}

long Scene::Transform3DTo2D(const Vector3 &ver3D, lPoint &point) {
  if (m_pCamera) m_pCamera->apply();

  if (NULL != m_p3DRenderDevice)
    m_p3DRenderDevice->Transform3DTo2D(ver3D, point);

  return kErrNone;
}

void Scene::CreateOctTreeSceneMgr(void) {
  m_pSceneTree->rebuild(m_v3DObjectPtrs);
  m_aAbb = m_pSceneTree->aabb();
  m_bOctTreeCreated = true;
  // SP4: one switch — mirror object AABBs into World when a mirror is set
  // (map_to_scene / tests). Does not delete leftover octree.
  if (vista::World *mirror = smt_scene_world_mirror()) {
    seed_smt_scene_aabbs_into_world(mirror, this);
  }
}

void Scene::Add3DObject(Object3d *p3DObject) {
  if (NULL != p3DObject) {
    m_v3DObjectPtrs.push_back(p3DObject);
    m_aAbb.merge(p3DObject->GetAabb());
    m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;
    m_bOctTreeCreated = false;
  }
}

void Scene::Remove3DObject(int index) {
  Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      SAFE_DELETE(*iter);
      m_v3DObjectPtrs.erase(iter);
      CreateOctTreeSceneMgr();
      break;
    }

    ++iter;
    --index;
  }
}

void Scene::Remove3DObject(Object3d *p3DObject) {
  Object3dPtrs::iterator iter;
  iter = find(m_v3DObjectPtrs.begin(), m_v3DObjectPtrs.end(), p3DObject);

  if (iter != m_v3DObjectPtrs.end()) {
    SAFE_DELETE(*iter);
    m_v3DObjectPtrs.erase(iter);
    CreateOctTreeSceneMgr();
  }
}

Object3d *Scene::Get3DObject(int index) {
  Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      return (*iter);
    }

    ++iter;
    --index;
  }

  return NULL;
}

const Object3d *Scene::Get3DObject(int index) const {
  Object3dPtrs ::const_iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      return (*iter);
    }

    ++iter;
    --index;
  }

  return NULL;
}

void Scene::Get3DObjectPtrs(Object3dPtrs &v3DObjectPtrs) {
  v3DObjectPtrs = m_v3DObjectPtrs;
}

long Scene::Select3DObject(Object3dPtrs &vSelected3DObjects,
                              lPoint point) {
  if (m_pCamera) m_pCamera->apply();

  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree && NULL != m_p3DRenderDevice)
      m_pSceneTree->select_objects(vSelected3DObjects, m_p3DRenderDevice,
                                   point);
  } else {
    Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        if ((*iter)->Select(m_p3DRenderDevice, point)) {
          vSelected3DObjects.push_back(*iter);
        }
      }

      iter++;
    }
  }

  return kErrNone;
}

long Scene::TransModel3DObjects(::base::Matrix &matTransform) {
  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree)
      m_pSceneTree->multiply_object_model_matrices(matTransform);
  } else {
    Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        (*iter)->ModelTransMatrixMultiply(matTransform);
      }
      iter++;
    }
  }

  return kErrNone;
}

long Scene::TransWorld3DObjects(::base::Matrix &matTransform) {
  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree)
      m_pSceneTree->multiply_object_world_matrices(matTransform);
  } else {
    Object3dPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        (*iter)->WorldTransMatrixMultiply(matTransform);
      }
      iter++;
    }
  }

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic