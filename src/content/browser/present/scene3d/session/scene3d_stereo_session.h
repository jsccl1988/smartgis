// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_STEREO_SESSION_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_STEREO_SESSION_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {

// Runtime LoadLibrary of scenic_impl (stereo HWND GL/D3D) without linking the
// DLL into product TUs. Prefer GPU capture_bgr24 over GDI BitBlt — double-
// buffered GL has no PFD_SUPPORT_GDI.
class Scene3dStereoSession {
 public:
  Scene3dStereoSession() = default;
  ~Scene3dStereoSession();

  Scene3dStereoSession(const Scene3dStereoSession&) = delete;
  Scene3dStereoSession& operator=(const Scene3dStereoSession&) = delete;

  // Bind scenic_impl stereo to |hwnd| (seed DEM + vectors + labels + HUD).
  bool try_attach(HWND hwnd);
  // Destroy view + FreeLibrary when destroy_ is still inside |module_| image.
  void release();
  // Drop handles without destroy_/FreeLibrary. Use under FlyCube-default SoT
  // when leftover GL may hold a stale destroy_ into remapped heap (AV).
  void abandon();
  bool is_live() const { return view_ != nullptr; }

  void resize(int width_px, int height_px);

  // Orbit params from OrbitFrame. SwapBuffers to HWND.
  bool present(float yaw, float pitch, float distance);

  // Present then BitBlt into |hdc| (WinUI / CEF / Cs paint_to_dc).
  bool present_to_dc(HDC hdc, int width_px, int height_px, float yaw,
                     float pitch, float distance);

  // Present then GPU readback into tightly packed bottom-up BGR24
  // (|out_bgr24| = width*height*3). Prefer over HWND BitBlt for showcase BMP.
  bool capture_bgr24(unsigned char* out_bgr24, int width_px, int height_px,
                     float yaw, float pitch, float distance);

  // Lazy attach + present_to_dc. Primary product Scene3d SoT when
  // scenic_impl[_d].dll is beside the PE.
  bool try_present_sot(HWND hwnd, HDC hdc, int width_px, int height_px,
                       float yaw, float pitch, float distance);

 private:
  using CreateFn = void* (*)(HWND);
  using DestroyFn = void (*)(void*);
  using ResizeFn = int (*)(void*, int, int);
  using PresentFn = int (*)(void*, float, float, float);
  using BlitFn = int (*)(void*, HDC, int, int);
  using CaptureFn = int (*)(void*, unsigned char*, int, int);

  HMODULE module_ = nullptr;
  void* view_ = nullptr;
  HWND host_ = nullptr;
  CreateFn create_ = nullptr;
  DestroyFn destroy_ = nullptr;
  ResizeFn resize_ = nullptr;
  PresentFn present_ = nullptr;
  BlitFn blit_ = nullptr;
  CaptureFn capture_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_STEREO_SESSION_H_
