// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_stereo_session.h"

namespace app {
namespace {

HMODULE load_legacy_render() {
#ifdef _DEBUG
  const wchar_t* names[] = {L"legacy_render_d.dll", L"legacy_render.dll",
                            nullptr};
#else
  const wchar_t* names[] = {L"legacy_render.dll", L"legacy_render_d.dll",
                            nullptr};
#endif
  for (const wchar_t** p = names; *p; ++p) {
    if (HMODULE m = LoadLibraryW(*p)) {
      return m;
    }
  }
  return nullptr;
}

}  // namespace

Scene3dStereoSession::~Scene3dStereoSession() {
  release();
}

bool Scene3dStereoSession::try_attach(HWND hwnd) {
  release();
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  module_ = load_legacy_render();
  if (!module_) {
    return false;
  }
  create_ = reinterpret_cast<CreateFn>(
      GetProcAddress(module_, "smt_stereo_hwnd_create"));
  destroy_ = reinterpret_cast<DestroyFn>(
      GetProcAddress(module_, "smt_stereo_hwnd_destroy"));
  resize_ = reinterpret_cast<ResizeFn>(
      GetProcAddress(module_, "smt_stereo_hwnd_resize"));
  present_ = reinterpret_cast<PresentFn>(
      GetProcAddress(module_, "smt_stereo_hwnd_present"));
  blit_ =
      reinterpret_cast<BlitFn>(GetProcAddress(module_, "smt_stereo_hwnd_blit"));
  if (!create_ || !destroy_ || !resize_ || !present_) {
    release();
    return false;
  }
  view_ = create_(hwnd);
  if (!view_) {
    release();
    return false;
  }
  RECT rc = {};
  GetClientRect(hwnd, &rc);
  if (rc.right > 0 && rc.bottom > 0) {
    resize_(view_, rc.right, rc.bottom);
  }
  return true;
}

void Scene3dStereoSession::release() {
  if (view_ && destroy_) {
    destroy_(view_);
  }
  view_ = nullptr;
  create_ = nullptr;
  destroy_ = nullptr;
  resize_ = nullptr;
  present_ = nullptr;
  blit_ = nullptr;
  if (module_) {
    FreeLibrary(module_);
    module_ = nullptr;
  }
}

void Scene3dStereoSession::resize(int width_px, int height_px) {
  if (view_ && resize_ && width_px > 0 && height_px > 0) {
    resize_(view_, width_px, height_px);
  }
}

bool Scene3dStereoSession::present(float yaw, float pitch, float distance) {
  if (!view_ || !present_) {
    return false;
  }
  return present_(view_, yaw, pitch, distance) != 0;
}

bool Scene3dStereoSession::present_to_dc(HDC hdc, int width_px, int height_px,
                                         float yaw, float pitch,
                                         float distance) {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return false;
  }
  resize(width_px, height_px);
  if (!present(yaw, pitch, distance)) {
    return false;
  }
  if (blit_) {
    return blit_(view_, hdc, width_px, height_px) != 0;
  }
  return true;
}

bool Scene3dStereoSession::try_present_sot(HWND hwnd, HDC hdc, int width_px,
                                           int height_px, float yaw,
                                           float pitch, float distance) {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (!is_live()) {
    if (!hwnd || !IsWindow(hwnd) || !try_attach(hwnd)) {
      return false;
    }
  }
  return present_to_dc(hdc, width_px, height_px, yaw, pitch, distance);
}

}  // namespace app
