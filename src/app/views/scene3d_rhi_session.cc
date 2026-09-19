// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_rhi_session.h"

#include "app/views/scene3d_controller.h"
#include "render/rhi/rhi.h"

#include <cstdlib>

namespace app {

bool force_content_mapview_3d() {
  if (const char* env = std::getenv("SMT_FORCE_CONTENT_MAPVIEW_3D")) {
    if (env[0] == '1' && env[1] == '\0') {
      return true;
    }
  }
  // Legacy invert: SMT_PREFER_FLYCUBE_3D=0 forces ContentMapView first.
  if (const char* prefer = std::getenv("SMT_PREFER_FLYCUBE_3D")) {
    if (prefer[0] == '0' && prefer[1] == '\0') {
      return true;
    }
  }
  return false;
}

bool prefer_scene3d_flycube() {
  return !force_content_mapview_3d();
}

Scene3dRhiSession::~Scene3dRhiSession() {
  release();
}

void Scene3dRhiSession::release() {
  last_ok_ = false;
  if (!device_) {
    return;
  }
  device_->shutdown();
  // FlyCube CRT / allocator can heap-corrupt on operator delete after a live
  // DX12 session; leak the facade (matches MapViewport / rhi_test).
  device_ = nullptr;
}

bool Scene3dRhiSession::try_attach(HWND hwnd) {
  if (const char* prefer = std::getenv("SMT_PREFER_GDI_DEVICE")) {
    if (prefer[0] == '1' && prefer[1] == '\0') {
      return false;
    }
  }
#ifndef SMT_HAS_FLYCUBE
  (void)hwnd;
  return false;
#else
  if (!hwnd) {
    return false;
  }
  release();
  render::rhi::Device* device =
      render::rhi::create_device(render::rhi::preferred_gpu_backend());
  if (!device) {
    return false;
  }
  RECT rc = {};
  GetClientRect(hwnd, &rc);
  render::rhi::DeviceDesc desc;
  desc.native_window = hwnd;
  desc.width = rc.right > 0 ? static_cast<uint32_t>(rc.right) : 1;
  desc.height = rc.bottom > 0 ? static_cast<uint32_t>(rc.bottom) : 1;
  if (!device->initialize(desc)) {
    device->shutdown();
    return false;
  }
  render::rhi::CommandList* list = device->create_command_list();
  if (list) {
    render::rhi::RenderPassDesc pass;
    pass.clear_r = 0.05f;
    pass.clear_g = 0.12f;
    pass.clear_b = 0.18f;
    pass.clear_a = 1.f;
    pass.width = desc.width;
    pass.height = desc.height;
    list->begin_render_pass(pass);
    list->set_viewport(0, 0, static_cast<float>(desc.width),
                       static_cast<float>(desc.height), 0, 1);
    list->end_render_pass();
    list->close();
    device->execute(list);
    device->destroy_command_list(list);
  }
  device->present();
  device_ = device;
  last_ok_ = true;
  return true;
#endif
}

bool Scene3dRhiSession::present(Scene3dController* cam,
                                uint32_t width_px,
                                uint32_t height_px) {
  last_ok_ = false;
  if (!cam || !device_ || width_px == 0 || height_px == 0) {
    return false;
  }
  last_ok_ = cam->present_gpu(device_, width_px, height_px);
  return last_ok_;
}

void Scene3dRhiSession::resize(HWND hwnd,
                               uint32_t width_px,
                               uint32_t height_px) {
  if (!device_ || !hwnd || width_px == 0 || height_px == 0) {
    return;
  }
  render::rhi::DeviceDesc desc;
  desc.native_window = hwnd;
  desc.width = width_px;
  desc.height = height_px;
  device_->initialize(desc);
}

}  // namespace app
