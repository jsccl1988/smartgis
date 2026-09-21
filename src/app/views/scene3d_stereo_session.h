// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SCENE3D_STEREO_SESSION_H_
#define APP_VIEWS_SCENE3D_STEREO_SESSION_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {

// Runtime LoadLibrary of leftover OpenGL stereo (legacy_render DLL) without
// linking leftover / MFC chrome (SP5). Visual SoT matches SmartGis.exe 3D.
class Scene3dStereoSession {
 public:
  Scene3dStereoSession() = default;
  ~Scene3dStereoSession();

  Scene3dStereoSession(const Scene3dStereoSession&) = delete;
  Scene3dStereoSession& operator=(const Scene3dStereoSession&) = delete;

  // Bind leftover GL stereo to |hwnd| (seed DEM + vectors + labels + HUD).
  bool try_attach(HWND hwnd);
  void release();
  bool is_live() const { return view_ != nullptr; }

  void resize(int width_px, int height_px);

  // Orbit params from Scene3dController. SwapBuffers to the attached HWND.
  bool present(float yaw, float pitch, float distance);

  // Present then BitBlt into |hdc| (WinUI / CEF / Cs paint_to_dc).
  bool present_to_dc(HDC hdc, int width_px, int height_px, float yaw,
                     float pitch, float distance);

  // Lazy attach + present_to_dc. Primary product Scene3d SoT when
  // legacy_render[_d].dll is beside the PE (SP5 LoadLibrary bridge).
  bool try_present_sot(HWND hwnd, HDC hdc, int width_px, int height_px,
                       float yaw, float pitch, float distance);

 private:
  using CreateFn = void* (*)(HWND);
  using DestroyFn = void (*)(void*);
  using ResizeFn = int (*)(void*, int, int);
  using PresentFn = int (*)(void*, float, float, float);
  using BlitFn = int (*)(void*, HDC, int, int);

  HMODULE module_ = nullptr;
  void* view_ = nullptr;
  CreateFn create_ = nullptr;
  DestroyFn destroy_ = nullptr;
  ResizeFn resize_ = nullptr;
  PresentFn present_ = nullptr;
  BlitFn blit_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SCENE3D_STEREO_SESSION_H_
