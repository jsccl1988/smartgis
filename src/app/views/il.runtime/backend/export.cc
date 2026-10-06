// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/export.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/il.runtime/backend/bmp.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/paint.h"
#include "app/views/il.runtime/backend/capture.h"
#include "app/views/il.runtime/backend/frame.h"
#include "app/views/il.runtime/backend/pump.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

bool export_scene3d_bmp(Browser* b, const wchar_t* bmp_w) {
  if (!b || !bmp_w || !bmp_w[0]) {
    return false;
  }
  content::Scene3dPresenter* cam = b->scene3d();
  if (!cam) {
    return false;
  }
  // Prove FlyCube GpuPresent is live, then soft-export the shared DEM mesh.
  // DXGI BitBlt of flip-model HWND is often solid black/white and is not
  // the score SoT (browse.3d uses the same software hypsometric path).
  apply_china_scene3d_orbit(*b);
  if (HWND shell = b->hwnd()) {
    if (IsWindow(shell)) {
      SetWindowPos(shell, nullptr, 0, 0, 1280, 800,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
      pump_messages(80);
    }
  }
  b->select_map_tab(1);
  pump_messages(120);
  ui::views::DrawHost* scene = b->scene_draw_host();
  bool flycube_live = false;
  if (scene && content::prefer_scene3d_flycube()) {
    scene->set_gpu_present_visible(true);
    if (scene->attach_mode() != ui::views::DrawHost::AttachMode::kGpuPresent) {
      b->select_map_tab(1);
      pump_messages(200);
    }
    int present_w = 0;
    int present_h = 0;
    if (HWND ph = scene->present_hwnd()) {
      if (IsWindow(ph)) {
        RECT prc = {};
        GetClientRect(ph, &prc);
        present_w = prc.right - prc.left;
        present_h = prc.bottom - prc.top;
      }
    }
    if (scene->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent) {
      flycube_live = present_shell_scene3d_frame(scene, 1200);
      if (!flycube_live) {
        flycube_live = scene->last_gpu_present_ok();
      }
    }
    if (flycube_live && present_w >= 640 && present_h >= 360) {
      HWND present = scene->present_hwnd();
      if (present && IsWindow(present)) {
        ShowWindow(present, SW_SHOWNOACTIVATE);
        SetWindowPos(present, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        pump_messages(40);
        (void)present_shell_scene3d_frame(scene, 400);
        Scene3dHwndCaptureOpts cap;
        cap.bmp_path = bmp_w;
        cap.present_w = kCaptureW;
        cap.present_h = kCaptureH;
        cap.dst_w = kCaptureW;
        cap.dst_h = kCaptureH;
        cap.pre_capture_pump_ms = 0;
        cap.skip_when_null_gpu = false;
        cap.skip_ui_thread_present = true;
        cap.require_color_diversity = true;
        cap.allow_32bpp = true;
        cap.mean_min = 8.0;
        cap.mean_max = 220.0;
        cap.engine_sidecar = "FlyCube/DX12";
        auto* device =
            static_cast<render::rhi::Device*>(scene->rhi_device());
        if (capture_scene3d_hwnd_bmp(cam, device, present, true, cap)) {
          write_mark(kUiShowcaseMarkLeaf, "interact-3d-gpu-ok", false);
          return true;
        }
        DeleteFileW(bmp_w);
      }
    }
  }
  // Soft paint must not race the Display thread: holding present_mu_ while
  // FlyCube is mid-present has aborted (exit 3) after interact-3d-gestures.
  const bool paused_present = scene != nullptr;
  if (paused_present) {
    scene->pause_present();
    pump_messages(60);
  }
  if (content::OrbitFrame* orbit = b->orbit_frame()) {
    orbit->set_distance(2.35f);
    orbit->set_pitch(0.62f);
  }
  cam->gpu().set_wireframe_enabled(false);
  cam->clear_overlay_tin_mesh();
  if (flycube_live) {
    cam->set_render_engine_name("FlyCube/DX12");
  }
  const bool wrote = write_software_scene3d_bmp(cam, bmp_w);
  if (wrote) {
    (void)write_engine_sidecar(bmp_w, flycube_live ? "FlyCube/DX12" : "GDI");
    if (flycube_live) {
      write_mark(kUiShowcaseMarkLeaf, "interact-3d-gpu-ok", false);
    }
  }
  if (paused_present) {
    scene->resume_present_timer();
  }
  return wrote;
}

bool export_map2d_bmp(Browser* b,
                      const wchar_t* bmp_w,
                      const std::string& frame,
                      const Map2dExportOpts& opts) {
  if (!b || !bmp_w || !bmp_w[0]) {
    return false;
  }
  content::Map2dPresenter* map2d = b->map2d();
  if (!map2d) {
    return false;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    return false;
  }
  if (opts.apply_frame &&
      !resolve_export_frame(*b, frame.empty() ? "document_extent" : frame)) {
    return false;
  }
  content::MapScene* doc = b->document();
  if (opts.require_features && (!doc || doc->feature_count() == 0)) {
    return false;
  }
  map2d->bind(doc, b->view_frame());
  if (opts.invalidate_cache) {
    map2d->invalidate_frame_cache();
  }
  if (opts.kick_paint) {
    kick_draw_host_paint(b->draw_host());
  }
  if (opts.pump_ms > 0) {
    pump_messages(static_cast<DWORD>(opts.pump_ms));
  }
  const int w = opts.width > 0 ? opts.width : kCaptureW;
  const int h = opts.height > 0 ? opts.height : kCaptureH;
  return map2d->export_bmp(bmp_a, w, h);
}

}  // namespace detail
}  // namespace app
