// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_PROJ_PROJ_BACKEND_H_
#define ALGORITHM_PROJ_PROJ_BACKEND_H_

#include "base/core/core.h"

#if defined(PROJ_EXPORTS)
#define PROJ_EXPORT_API __declspec(dllexport)
#else
#define PROJ_EXPORT_API __declspec(dllimport)
#endif

namespace proj {

// CRS-to-CRS XY transform via the shipped PROJ 9 runtime. Strings are
// EPSG / WKT / PROJ. `x` and `y` are updated in place.
PROJ_EXPORT_API long transform_xy(const char* src_crs,
                                  const char* dst_crs,
                                  double* x,
                                  double* y);

}  // namespace proj

#endif  // ALGORITHM_PROJ_PROJ_BACKEND_H_
