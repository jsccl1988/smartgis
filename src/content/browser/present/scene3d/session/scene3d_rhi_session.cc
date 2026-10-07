// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"

#include <atomic>
#include <cstdlib>
#include <cstring>

#include "base/process/switches.h"

#if defined(_WIN32)
#include <windows.h>  // _stricmp, _putenv_s
#else
#include <strings.h>  // strcasecmp
#endif

namespace content {
namespace {

std::atomic<uint32_t> g_scene3d_engine{
    static_cast<uint32_t>(Scene3dEngine::kFlyCube)};

bool env_eq_ci(const char* a, const char* b) {
  if (!a || !b) {
    return false;
  }
#if defined(_WIN32)
  return _stricmp(a, b) == 0;
#else
  return std::strcasecmp(a, b) == 0;
#endif
}

// Mirror leftover stereo_hwnd_view / legacy showcase: default D3D11.
bool stereo_api_is_d3d_from_env() {
  if (const char* api = base::switch_cstr("stereo-api")) {
    if (env_eq_ci(api, "OpenGL")) {
      return false;
    }
    if (env_eq_ci(api, "Direct3D")) {
      return true;
    }
  }
  if (const char* flag = base::switch_cstr("scene3d-showcase-d3d")) {
    if (flag[0] == '0' || flag[0] == 'n' || flag[0] == 'N') {
      return false;
    }
    if (flag[0] == '1' || flag[0] == 'y' || flag[0] == 'Y') {
      return true;
    }
  }
  return true;
}

void set_stereo_api_env(bool want_d3d) {
#if defined(_WIN32)
  base::set_switch("stereo-api", want_d3d ? "Direct3D" : "OpenGL");
  base::set_switch("scene3d-showcase-d3d", want_d3d ? "1" : "0");
#else
  (void)want_d3d;
#endif
}

}  // namespace

void set_scene3d_engine(Scene3dEngine engine) {
  g_scene3d_engine.store(static_cast<uint32_t>(engine),
                         std::memory_order_release);
}

Scene3dEngine scene3d_engine() {
  return static_cast<Scene3dEngine>(
      g_scene3d_engine.load(std::memory_order_acquire));
}

bool apply_scene3d_engine_from_env() {
  // Do not enable GPUSCENE_PREP_PARALLEL here. Product default remains off
  // until frustum cull honesty (vista prep_cull / equal-profile M3).
  // Switch wins when both are set. Harness suite.json often only exports
  // SCENE3D_ENGINE; loop_runner peel_product_switches does not run for
  // SmartGIS.exe, so getenv is required.
  const char* raw = base::switch_cstr("scene3d-engine");
  if (!raw || !raw[0]) {
    raw = std::getenv("SCENE3D_ENGINE");
  }
  if (!raw || !raw[0]) {
    return false;
  }
  if (env_eq_ci(raw, "flycube") || env_eq_ci(raw, "dx12")) {
    set_scene3d_engine(Scene3dEngine::kFlyCube);
    return true;
  }
  if (env_eq_ci(raw, "gdi")) {
    set_scene3d_engine(Scene3dEngine::kGdi);
    return true;
  }
  if (env_eq_ci(raw, "scenic")) {
    set_scene3d_engine(Scene3dEngine::kScenic);
    return true;
  }
  if (env_eq_ci(raw, "stereo_gl") || env_eq_ci(raw, "opengl") ||
      env_eq_ci(raw, "gl")) {
    set_scene3d_engine(Scene3dEngine::kStereoGl);
    set_stereo_api_env(/*want_d3d=*/false);
    return true;
  }
  if (env_eq_ci(raw, "stereo_d3d") || env_eq_ci(raw, "d3d") ||
      env_eq_ci(raw, "direct3d") || env_eq_ci(raw, "d3d11")) {
    set_scene3d_engine(Scene3dEngine::kStereoGl);
    set_stereo_api_env(/*want_d3d=*/true);
    return true;
  }
  return false;
}

bool prefer_scene3d_flycube() {
  return scene3d_engine() == Scene3dEngine::kFlyCube;
}

bool prefer_scene3d_stereo_gl() {
  return scene3d_engine() == Scene3dEngine::kStereoGl;
}

bool prefer_scene3d_stereo_opengl() {
  return prefer_scene3d_stereo_gl() && !stereo_api_is_d3d_from_env();
}

bool prefer_scene3d_stereo_d3d() {
  return prefer_scene3d_stereo_gl() && stereo_api_is_d3d_from_env();
}

bool prefer_scene3d_gdi() {
  return scene3d_engine() == Scene3dEngine::kGdi;
}

bool prefer_scene3d_scenic() {
  return scene3d_engine() == Scene3dEngine::kScenic;
}

bool force_content_mapview_3d() {
  // Only leftover stereo needs a ContentMapView HWND. Scenic/GDI paint
  // Scene3dPresenter on the product HWND — ContentMapView SharedSurface
  // plus overlay fill flashes navy every present tick.
  return prefer_scene3d_stereo_gl();
}

}  // namespace content
