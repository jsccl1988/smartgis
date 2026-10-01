// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_MAP2D_PHASE_PROFILE_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_MAP2D_PHASE_PROFILE_H_

#include <cstdint>

#include "content/content_export.h"

namespace content {

// Last equal-profile phase sample for harness / matrix fair compare.
// layout + hillshade nest under rebuild; software_paint is GDI after prepare;
// bmp_io is file write; gpu_upload / gpu_present split Pass record vs swap.
struct Map2dPhaseSample {
  int64_t layout_ms = 0;
  int64_t hillshade_ms = 0;
  int64_t software_paint_ms = 0;
  int64_t bmp_io_ms = 0;
  int64_t gpu_upload_ms = 0;
  int64_t gpu_present_ms = 0;
};

CONTENT_EXPORT Map2dPhaseSample map2d_last_phase_sample();
CONTENT_EXPORT void reset_map2d_phase_sample();

CONTENT_EXPORT void note_map2d_phase_layout(int64_t layout_ms,
                                           int64_t hillshade_ms);
CONTENT_EXPORT void note_map2d_phase_software_paint(int64_t paint_ms);
CONTENT_EXPORT void note_map2d_phase_bmp_io(int64_t bmp_io_ms);
CONTENT_EXPORT void note_map2d_phase_gpu(int64_t upload_ms, int64_t present_ms);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_MAP2D_PHASE_PROFILE_H_
