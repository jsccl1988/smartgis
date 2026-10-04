// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"

#include <atomic>
#include <cstdlib>
#include <cstring>

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
  if (const char* api = std::getenv("SMT_STEREO_API")) {
    if (env_eq_ci(api, "OpenGL")) {
      return false;
    }
    if (env_eq_ci(api, "Direct3D")) {
      return true;
    }
  }
  if (const char* flag = std::getenv("SMT_SCENE3D_SHOWCASE_D3D")) {
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
  _putenv_s("SMT_STEREO_API", want_d3d ? "Direct3D" : "OpenGL");
  _putenv_s("SMT_SCENE3D_SHOWCASE_D3D", want_d3d ? "1" : "0");
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
  const char* raw = std::getenv("SMT_SCENE3D_ENGINE");
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
  // Scenic keeps the product HWND presenter; only stereo/GDI force the
  // content MapView 3D path.
  return prefer_scene3d_stereo_gl() || prefer_scene3d_gdi();
}

}  // namespace content
