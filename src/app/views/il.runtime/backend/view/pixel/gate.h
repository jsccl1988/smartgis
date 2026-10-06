// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_PIXEL_GATE_H_
#define IL_RUNTIME_BACKEND_VIEW_PIXEL_GATE_H_

#include "app/views/il.runtime/backend/view/pixel/bmp.h"

namespace app {
namespace detail {

// Pixel lit-signal policies used by HWND capture and BMP file checks.
enum class VisiblePolicy {
  // Atmosphere / map2d: grid sample, >=5% non-near-black.
  kGridLitFraction,
  // UI horizon: sparse sample, enough lit + distinct colors.
  kSparseDistinct,
};

// Mean / unique-color gate on an already-decoded 24- or 32-bpp buffer.
// Lit-fraction (>=5%) is always required. Mean bounds are the open interval
// (mean_min, mean_max); 0 disables that side.
struct PixelGate {
  bool require_color_diversity = false;
  double mean_min = 0.0;
  double mean_max = 0.0;
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

// Grid lit-fraction plus optional mean and unique-color checks.
// |bpp| is 3 (BGR) or 4 (BGRA). A short stride fails closed.
bool pixels_pass_gate(const unsigned char* pixels,
                      int stride,
                      int w,
                      int h,
                      int bpp,
                      const PixelGate& gate);

// File-level accept: size, bit depth, then the pixel gate.
struct BmpFileCheckOpts {
  int min_w = 320;
  int min_h = 240;
  bool allow_32bpp = false;
  // Atmosphere: reject flat clear-color frames (unique colors < 2).
  bool require_color_diversity = false;
  // 0 = no bound. DXGI flip-model reject: mean in (mean_min, mean_max).
  double mean_min = 0.0;
  double mean_max = 0.0;
};

// Wide-path BMP file check (atmosphere).
bool bmp_file_has_visible_signal(const wchar_t* filename,
                                 int* out_w,
                                 int* out_h,
                                 const BmpFileCheckOpts& opts = {});

// ANSI-path BMP file check (map2d export_bmp). fopen_s uses the ACP.
bool bmp_file_has_visible_signal_a(const char* filename,
                                   int* out_w,
                                   int* out_h,
                                   const BmpFileCheckOpts& opts = {});

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_PIXEL_GATE_H_
