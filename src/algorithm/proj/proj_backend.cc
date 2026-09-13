// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/proj/proj_backend.h"

#include "algorithm/proj/proj_backend_traits.h"

namespace proj {

long transform_xy(const char* src_crs,
                  const char* dst_crs,
                  double* x,
                  double* y) {
  if (src_crs == nullptr || src_crs[0] == '\0' || dst_crs == nullptr ||
      dst_crs[0] == '\0' || x == nullptr || y == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  struct Xy {
    double x;
    double y;
  };
  Xy point{*x, *y};
  const int rc = proj_backend_traits<Xy>::transform(src_crs, dst_crs, point);
  if (rc != SMT_ERR_NONE) {
    return rc;
  }
  *x = point.x;
  *y = point.y;
  return SMT_ERR_NONE;
}

}  // namespace proj
