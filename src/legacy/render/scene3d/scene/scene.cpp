#include "legacy/render/scene3d/scene/scene.h"

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
#include "legacy/core/types/types.h"
#include "legacy/render/detail/frame_pipeline.h"
#include "legacy/render/rhi3d/impl/common/frame/prep_runner.h"
#include "legacy/render/scene3d/scene/d3d_deferred_objects.h"
#include "legacy/render/scene3d/seed/scene_to_world.h"

namespace render {
namespace {

bool object_aabb_in_frustum(const Frustum& frustum, Smt3DObject* obj) {
  if (!obj) {
    return false;
  }
  return frustum.intersects(obj->GetAabb());
}

}  // namespace
SmtScene::SmtScene(void)
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

SmtScene::~SmtScene(void) {
  vSmt3DObjectPtrs::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    SMT_SAFE_DELETE(*iter);
    ++iter;
  }

  SMT_SAFE_DELETE(m_pNorthArray);
  SMT_SAFE_DELETE(m_pSceneTree);
  SMT_SAFE_DELETE(m_pTimer);

  m_pCamera = NULL;
  m_p3DRenderDevice = NULL;
}

inline void SmtScene::SetSceneCamera(SmtPerspCamera *pCamera) {
  m_pCamera = pCamera;
  if (m_pNorthArray) m_pNorthArray->SetPerspCamera(m_pCamera);
}

long SmtScene::Setup() {
  m_pSceneTree = new SmtSceneOctTree();
  m_pTimer = new ::base::FrameTimer();
  m_pNorthArray = new SmtNorthArray(90, 120, NULL);
  Vector3 north_origin(0, 0, 0);
  SmtMaterial north_mat;
  if (m_pNorthArray->Init(north_origin, north_mat) == SMT_ERR_NONE) {
    m_pNorthArray->Create(m_p3DRenderDevice);
  }

  m_pTimer->set_clock(0, 0);

  m_pSceneTree->SetShowNodeBox(m_bShowNodeBox);

  m_p3DRenderDevice->CreateFont("Calibri", 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                16, m_nRenderInfoFont);
  m_p3DRenderDevice->CreateFont("MS Reference Sans Serif", 16, 0, FW_BOLD,
                                FALSE, FALSE, FALSE, 16, m_nTimerInfoFont);
  m_p3DRenderDevice->CreateFont("Times New Roman", 16, 0, FW_BOLD, FALSE, FALSE,
                                FALSE, 10, m_nHelpInfoFont);

  LOGGING(LOG_INFO, "SmtScene::Setup() is ok!");

  return SMT_ERR_NONE;
}

long SmtScene::Update() {
  BASE_TRACE_EVENT("Update", "scene3d");
  if (m_pTimer && m_pCamera && m_pSceneTree) {
    m_pTimer->update();

    sprintf(m_szHelpInfoBuf,
            "Move:Front:W  Left:A  Back:S  Right:D Eye : X%f  Y:%f   Z:%f ",
            m_pCamera->eye().x, m_pCamera->eye().y,
            m_pCamera->eye().z);

    if (m_bOctTreeCreated) {
      m_pSceneTree->SetShowNodeBox(m_bShowNodeBox);

      m_pSceneTree->Update(m_p3DRenderDevice, m_pTimer->get_elapsed());

    } else {
      vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
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

  return SMT_ERR_NONE;
}

long SmtScene::Render(void) {
  BASE_TRACE_EVENT("Render", "scene3d");
  detail::log_legacy_flow("scene3d.Render");
  if (NULL != m_pSceneTree && NULL != m_p3DRenderDevice && m_pTimer != NULL) {
    if (m_pCamera) m_pCamera->apply();

    // scene object
    if (m_bOctTreeCreated) {
      static char szBuf[TEMP_BUFFER_SIZE];

      if (m_pSceneTree->IsVisible()) {
        m_pSceneTree->Render(m_p3DRenderDevice);
        m_pSceneTree->GetDebugString(szBuf, TEMP_BUFFER_SIZE);
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
        const char* e = std::getenv("SMT_RHI3D_SKIP_FRUSTUM");
        return e && e[0] && e[0] != '0' && e[0] != 'n' && e[0] != 'N';
      }();
      {
        BASE_TRACE_EVENT("frustum_cull", "rhi3d.prep");
        detail::Rhi3dPrepRunner& prep = detail::rhi3d_shared_prep_runner();
        prep.ensure_workers(detail::rhi3d_prep_worker_count());
        const vSmt3DObjectPtrs& objects = m_v3DObjectPtrs;
        prep.run_jobs(n, [&](size_t i) {
          Smt3DObject* obj = objects[i];
          if (!obj || !obj->IsVisible()) {
            return;
          }
          if (skip_frustum || object_aabb_in_frustum(frustum, obj)) {
            in_frustum[i] = 1;
          }
        });
      }
      std::vector<Smt3DObject*> visible;
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
        const char* e = std::getenv("SMT_RHI3D_TIME_PRESENT");
        return e && e[0] == '1';
      }();
      if (time_draw) {
        QueryPerformanceFrequency(&qpf);
        QueryPerformanceCounter(&t_draw0);
      }
      std::vector<Smt3DObject*> immediate_after;
      if (!detail::render_objects_d3d_deferred(m_p3DRenderDevice, visible,
                                               &immediate_after)) {
        for (Smt3DObject* obj : visible) {
          obj->Render(m_p3DRenderDevice);
        }
      } else if (!immediate_after.empty()) {
        // Restore camera matrices before screen-space labels (P3 may race MV).
        if (m_pCamera) {
          m_pCamera->apply();
        }
        for (Smt3DObject* obj : immediate_after) {
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
      const char* e = std::getenv("SMT_RHI3D_DEBUG_HUD");
      if (!e || !e[0]) {
        return false;
      }
      return !(e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
               e[0] == 'F');
    }();
    if (draw_debug_hud) {
      // 3d text
      m_p3DRenderDevice->DrawText(m_nTimerInfoFont, 0, 0, 0, SmtColor(0., 1., 0.),
                                  "(0,0,0)");

      // 2d text
      m_p3DRenderDevice->DrawText(m_nHelpInfoFont, 10, 24, SmtColor(0., 0., 1.),
                                  m_szHelpInfoBuf);
      m_p3DRenderDevice->DrawText(m_nRenderInfoFont, 10, 44, SmtColor(0., 1., 0.),
                                  m_szRenderInfoBuf);
      m_p3DRenderDevice->DrawText(m_nTimerInfoFont, 10, 64, SmtColor(1., 1., 0.),
                                  m_pTimer->get_clock());
    }
  }

  return SMT_ERR_NONE;
}
long SmtScene::Transform2DTo3D(::base::Vector3 &vOrg, ::base::Vector3 &vTar,
                               const lPoint &point) {
  if (m_pCamera) m_pCamera->apply();

  if (NULL != m_p3DRenderDevice)
    m_p3DRenderDevice->Transform2DTo3D(vOrg, vTar, point);

  return SMT_ERR_NONE;
}

long SmtScene::Transform3DTo2D(const Vector3 &ver3D, lPoint &point) {
  if (m_pCamera) m_pCamera->apply();

  if (NULL != m_p3DRenderDevice)
    m_p3DRenderDevice->Transform3DTo2D(ver3D, point);

  return SMT_ERR_NONE;
}

void SmtScene::CreateOctTreeSceneMgr(void) {
  m_pSceneTree->CreateOctTree(m_v3DObjectPtrs);
  m_aAbb = m_pSceneTree->m_aabbScene;
  m_bOctTreeCreated = true;
  // SP4: one switch — mirror object AABBs into World when a mirror is set
  // (map_to_scene / tests). Does not delete leftover octree.
  if (gis::World *mirror = smt_scene_world_mirror()) {
    seed_smt_scene_aabbs_into_world(mirror, this);
  }
}

void SmtScene::Add3DObject(Smt3DObject *p3DObject) {
  if (NULL != p3DObject) {
    m_v3DObjectPtrs.push_back(p3DObject);
    m_aAbb.merge(p3DObject->GetAabb());
    m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.;
    m_bOctTreeCreated = false;
  }
}

void SmtScene::Remove3DObject(int index) {
  vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      SMT_SAFE_DELETE(*iter);
      m_v3DObjectPtrs.erase(iter);
      CreateOctTreeSceneMgr();
      break;
    }

    ++iter;
    --index;
  }
}

void SmtScene::Remove3DObject(Smt3DObject *p3DObject) {
  vSmt3DObjectPtrs::iterator iter;
  iter = find(m_v3DObjectPtrs.begin(), m_v3DObjectPtrs.end(), p3DObject);

  if (iter != m_v3DObjectPtrs.end()) {
    SMT_SAFE_DELETE(*iter);
    m_v3DObjectPtrs.erase(iter);
    CreateOctTreeSceneMgr();
  }
}

Smt3DObject *SmtScene::Get3DObject(int index) {
  vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      return (*iter);
    }

    ++iter;
    --index;
  }

  return NULL;
}

const Smt3DObject *SmtScene::Get3DObject(int index) const {
  vSmt3DObjectPtrs ::const_iterator iter = m_v3DObjectPtrs.begin();
  while (iter != m_v3DObjectPtrs.end()) {
    if (index == 0) {
      return (*iter);
    }

    ++iter;
    --index;
  }

  return NULL;
}

void SmtScene::Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs) {
  v3DObjectPtrs = m_v3DObjectPtrs;
}

long SmtScene::Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects,
                              lPoint point) {
  if (m_pCamera) m_pCamera->apply();

  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree && NULL != m_p3DRenderDevice)
      m_pSceneTree->Select3DObject(vSelected3DObjects, m_p3DRenderDevice,
                                   point);
  } else {
    vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        if ((*iter)->Select(m_p3DRenderDevice, point)) {
          vSelected3DObjects.push_back(*iter);
        }
      }

      iter++;
    }
  }

  return SMT_ERR_NONE;
}

long SmtScene::TransModel3DObjects(::base::Matrix &matTransform) {
  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree)
      m_pSceneTree->ObjectModelMatrixMultiply(matTransform);
  } else {
    vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        (*iter)->ModelTransMatrixMultiply(matTransform);
      }
      iter++;
    }
  }

  return SMT_ERR_NONE;
}

long SmtScene::TransWorld3DObjects(::base::Matrix &matTransform) {
  if (m_bOctTreeCreated) {
    if (NULL != m_pSceneTree)
      m_pSceneTree->ObjectWordlMatrixMultiply(matTransform);
  } else {
    vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
    while (iter != m_v3DObjectPtrs.end()) {
      if (NULL != (*iter)) {
        (*iter)->WorldTransMatrixMultiply(matTransform);
      }
      iter++;
    }
  }

  return SMT_ERR_NONE;
}
}  // namespace render