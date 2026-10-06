// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_CAPTURE_BMP_H_
#define APP_VIEWS_HARNESS_COMMON_CAPTURE_BMP_H_

#include <windows.h>

namespace app {
namespace detail {

// Pixel lit-signal policies used by HWND capture and BMP file checks.
enum class VisiblePolicy {
  // Atmosphere / map2d: grid sample, >=5% non-near-black.
  kGridLitFraction,
  // UI horizon: sparse sample, enough lit + distinct colors.
  kSparseDistinct,
};

struct CaptureOpts {
  int max_attempts = 1;
  DWORD pump_base_ms = 0;
  DWORD pump_step_ms = 0;
  bool require_shell_diversity = false;
  VisiblePolicy visible = VisiblePolicy::kGridLitFraction;
};

struct BmpFileCheckOpts {
  int min_w = 320;
  int min_h = 240;
  bool allow_32bpp = false;
  // Atmosphere: reject flat clear-color frames (unique colors < 2).
  bool require_color_diversity = false;
};

bool pixels_have_visible_signal(const unsigned char* pixels,
                                int stride,
                                int w,
                                int h,
                                VisiblePolicy policy);

bool bmp_has_shell_diversity(const unsigned char* pixels,
                              int stride,
                              int w,
                              int h);

bool blit_client_to_dib(HWND hwnd, HDC mem, int w, int h);

bool capture_hwnd_bmp(HWND hwnd,
                      const wchar_t* filename,
                      const CaptureOpts& opts = {});

// Wide-path BMP file check (atmosphere).
bool bmp_file_has_visible_signal(const wchar_t* filename,
                                 int* out_w,
                                 int* out_h,
                                 const BmpFileCheckOpts& opts = {});

// ANSI-path BMP file check (map2d export_bmp).
bool bmp_file_has_visible_signal_a(const char* filename,
                                   int* out_w,
                                   int* out_h,
                                   const BmpFileCheckOpts& opts = {});

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_CAPTURE_BMP_H_
