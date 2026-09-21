// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/stereo_hwnd_view.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include "gis/world/dem_frame.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/camera.h"
#include "legacy/render/scene3d/bl3d_scene.h"
#include "legacy/render/scene3d/map_to_scene.h"

namespace {

using render::Aabb;
using render::LP3DRENDERDEVICE;
using render::SmtColor;
using render::SmtLight;
using render::SmtPerspCamera;
using render::SmtScene;
using render::Vector3;
using render::Vector4;
using render::Viewport3D;

struct StereoHwndView {
  HMODULE module = nullptr;
  LP3DRENDERDEVICE device = nullptr;
  std::unique_ptr<SmtScene> scene;
  std::unique_ptr<SmtPerspCamera> camera;
  HWND hwnd = nullptr;
  int width = 0;
  int height = 0;
  Vector3 target{};
  float span = 40.f;
};

void setup_device_lights(LP3DRENDERDEVICE device) {
  if (!device) {
    return;
  }
  SmtLight lgt;
  lgt.SetType(render::LGT_DIRECTIONAL);
  lgt.SetAmbientValue(SmtColor(0.f, 0.f, 0.f, 1.f));
  lgt.SetDiffuseValue(SmtColor(1.f, 1.f, 1.f, 1.f));
  lgt.SetSpecularValue(SmtColor(0.f, 0.f, 0.f, 1.f));
  lgt.SetPoistion(Vector4(1.f, 1.f, 1.f, 0.f));
  lgt.SetDirection(Vector4(0.f, -1.f, -1.f, 0.f));
  device->SetLight(0, &lgt);
  lgt.SetDirection(Vector4(-1.f, -1.f, -1.f, 0.f));
  device->SetLight(1, &lgt);

  device->SetClearColor(SmtColor(0.f, 0.f, 0.f, 1.f));
  device->SetDepthClearValue(1.f);
  device->SetStencilClearValue(0);
  device->SetFog(render::FM_NONE, SmtColor(0.8f, 0.8f, 0.8f), 0.1f, 30.f,
                 100.f);
  device->SetBackfaceCulling(render::RSV_CULL_NONE);
  device->SetDepthBufferMode(render::RSV_DEPTH_READWRITE);
  device->SetShadeMode(render::RSV_SHADE_SOLID, 0, SmtColor(1.f, 1.f, 1.f, 1.f));
  device->SetAmbientLight(SmtColor(1.f, 1.f, 1.f, 1.f));
}

void apply_orbit_camera(StereoHwndView* v, float yaw, float pitch,
                        float distance) {
  if (!v || !v->camera) {
    return;
  }
  Aabb dem;
  Aabb use;
  if (render::leftover_dem_aabb(&dem) && dem.is_init()) {
    use = dem;
  } else if (v->scene) {
    use = v->scene->GetAabb();
  }
  Vector3 base_eye;
  Vector3 target;
  float span = 40.f;
  render::leftover_frame_pose(use, &base_eye, &target, &span);
  v->target = target;
  v->span = span;

  // Map Scene3dController orbit onto leftover geographic framing. Default
  // chrome yaw/pitch/distance reproduce leftover_frame_pose (south-of-target).
  const float yaw0 = gis::kDemDefaultOrbitYaw;
  const float pitch0 = 0.4f;
  const float dist0 = 3.2f;
  const Vector3 base_off = base_eye - target;
  const float base_len =
      std::sqrt(base_off.x * base_off.x + base_off.y * base_off.y +
                base_off.z * base_off.z);
  const float len =
      base_len * (distance > 0.05f ? (distance / dist0) : 1.f);
  const float dyaw = yaw - yaw0;
  const float dpitch = pitch - pitch0;

  // Rotate the leftover south offset around +Y by dyaw, then nudge pitch.
  const float cos_y = std::cos(dyaw);
  const float sin_y = std::sin(dyaw);
  float ox = base_off.x * cos_y + base_off.z * sin_y;
  float oy = base_off.y;
  float oz = -base_off.x * sin_y + base_off.z * cos_y;
  // Pitch: rotate offset in the vertical plane containing the look direction.
  const float horiz = std::sqrt(ox * ox + oz * oz);
  const float cos_p = std::cos(dpitch);
  const float sin_p = std::sin(dpitch);
  const float nh = horiz * cos_p - oy * sin_p;
  oy = horiz * sin_p + oy * cos_p;
  if (horiz > 1e-3f) {
    ox = ox * (nh / horiz);
    oz = oz * (nh / horiz);
  }
  Vector3 off(ox, oy, oz);
  const float olen =
      std::sqrt(off.x * off.x + off.y * off.y + off.z * off.z);
  if (olen > 1e-3f) {
    const float s = len / olen;
    off.x *= s;
    off.y *= s;
    off.z *= s;
  }
  Vector3 eye = target + off;
  Vector3 up(0.f, 1.f, 0.f);
  v->camera->SetETU(eye, target, up);
  v->camera->SetMoveStep(span / 100.f);
}

bool resize_view(StereoHwndView* v, int width_px, int height_px) {
  if (!v || !v->device || width_px <= 0 || height_px <= 0) {
    return false;
  }
  v->width = width_px;
  v->height = height_px;
  Viewport3D vp = v->device->GetViewport();
  render::apply_view3d_viewport(&vp, static_cast<ulong>(width_px),
                                static_cast<ulong>(height_px));
  if (v->device->SetViewport(vp) != SMT_ERR_NONE) {
    return false;
  }
  if (v->camera) {
    v->camera->SetViewport(vp);
  }
  return true;
}

}  // namespace

extern "C" {

void* smt_stereo_hwnd_create(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return nullptr;
  }
  auto view = std::make_unique<StereoHwndView>();
  view->hwnd = hwnd;

  HMODULE self = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&smt_stereo_hwnd_create), &self) ||
      !self) {
    return nullptr;
  }
  view->module = self;

  auto create_dev = reinterpret_cast<render::_Create3DRenderDevice>(
      GetProcAddress(self, "Create3DRenderDevice"));
  auto release_dev = reinterpret_cast<render::_Release3DRenderDevice>(
      GetProcAddress(self, "Release3DRenderDevice"));
  if (!create_dev || !release_dev) {
    return nullptr;
  }

  LP3DRENDERDEVICE device = nullptr;
  if (create_dev(self, device) != 0 || !device) {
    return nullptr;
  }
  view->device = device;
  if (device->Init(hwnd, "smt-stereo-hwnd") != SMT_ERR_NONE) {
    release_dev(device);
    return nullptr;
  }
  setup_device_lights(device);

  view->scene = std::make_unique<SmtScene>();
  view->scene->Set3DRenderDevice(device);
  if (view->scene->Setup() != SMT_ERR_NONE) {
    release_dev(device);
    view->device = nullptr;
    return nullptr;
  }

  const int seeded =
      render::seed_sample_map_into_scene(device, view->scene.get());
  if (seeded <= 0 && !render::leftover_has_scene_dem()) {
    release_dev(device);
    view->device = nullptr;
    return nullptr;
  }

  RECT rc = {};
  GetClientRect(hwnd, &rc);
  int w = rc.right > 0 ? rc.right : 400;
  int h = rc.bottom > 0 ? rc.bottom : 300;
  Viewport3D vp = device->GetViewport();
  render::apply_view3d_viewport(&vp, static_cast<ulong>(w),
                                static_cast<ulong>(h));
  device->SetViewport(vp);

  view->camera = std::make_unique<SmtPerspCamera>(device, vp);
  render::frame_persp_camera_to_aabb(view->camera.get(), &vp,
                                     view->scene->GetAabb());
  view->scene->SetSceneCamera(view->camera.get());
  view->width = w;
  view->height = h;
  apply_orbit_camera(view.get(), gis::kDemDefaultOrbitYaw, 0.4f, 3.2f);

  return view.release();
}

void smt_stereo_hwnd_destroy(void* view) {
  auto* v = static_cast<StereoHwndView*>(view);
  if (!v) {
    return;
  }
  v->camera.reset();
  v->scene.reset();
  if (v->device && v->module) {
    auto release_dev = reinterpret_cast<render::_Release3DRenderDevice>(
        GetProcAddress(v->module, "Release3DRenderDevice"));
    if (release_dev) {
      release_dev(v->device);
    }
    v->device = nullptr;
  }
  delete v;
}

int smt_stereo_hwnd_resize(void* view, int width_px, int height_px) {
  return resize_view(static_cast<StereoHwndView*>(view), width_px, height_px)
             ? 1
             : 0;
}

int smt_stereo_hwnd_present(void* view, float yaw, float pitch,
                            float distance) {
  auto* v = static_cast<StereoHwndView*>(view);
  if (!v || !v->device || !v->scene || !v->camera) {
    return 0;
  }
  RECT rc = {};
  if (v->hwnd) {
    GetClientRect(v->hwnd, &rc);
  }
  if (rc.right > 0 && rc.bottom > 0 &&
      (rc.right != v->width || rc.bottom != v->height)) {
    resize_view(v, rc.right, rc.bottom);
  } else if (v->width <= 0 || v->height <= 0) {
    resize_view(v, rc.right > 0 ? rc.right : 1, rc.bottom > 0 ? rc.bottom : 1);
  }
  apply_orbit_camera(v, yaw, pitch, distance);
  Viewport3D vp = v->device->GetViewport();
  render::apply_view3d_viewport(&vp, static_cast<ulong>(v->width),
                                static_cast<ulong>(v->height));
  v->device->SetViewport(vp);
  v->camera->SetViewport(vp);

  v->device->SetClearColor(SmtColor(0.f, 0.f, 0.f, 1.f));
  if (v->device->Clear(CLR_COLOR | CLR_ZBUFFER) != SMT_ERR_NONE) {
    return 0;
  }
  if (v->device->BeginRender() != SMT_ERR_NONE) {
    return 0;
  }
  v->scene->Update();
  v->scene->Render();
  if (v->device->EndRender() != SMT_ERR_NONE) {
    return 0;
  }
  return v->device->SwapBuffers() == SMT_ERR_NONE ? 1 : 0;
}

int smt_stereo_hwnd_blit(void* view, HDC hdc, int width_px, int height_px) {
  auto* v = static_cast<StereoHwndView*>(view);
  if (!v || !hdc || !v->hwnd || width_px <= 0 || height_px <= 0) {
    return 0;
  }
  HDC src = GetDC(v->hwnd);
  if (!src) {
    return 0;
  }
  const BOOL ok =
      BitBlt(hdc, 0, 0, width_px, height_px, src, 0, 0, SRCCOPY);
  ReleaseDC(v->hwnd, src);
  return ok ? 1 : 0;
}

}  // extern "C"
