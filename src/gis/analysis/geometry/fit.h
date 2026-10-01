// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOMETRY_FIT_H_
#define GIS_ANALYSIS_GEOMETRY_FIT_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// 2D least-squares line via SVD of centered points.
struct FitLineResult {
  bool ok = false;
  double cx = 0.0;
  double cy = 0.0;
  // Unit direction of the principal axis.
  double dx = 1.0;
  double dy = 0.0;
  double rms = 0.0;
  std::string error;
};

// 3D plane ax+by+cz+d=0 (unit normal), fitted by SVD.
struct FitPlaneResult {
  bool ok = false;
  double a = 0.0;
  double b = 0.0;
  double c = 1.0;
  double d = 0.0;
  double rms = 0.0;
  std::string error;
};

// 2D affine: x' = m0*x + m1*y + m2; y' = m3*x + m4*y + m5.
struct AffineAlignResult {
  bool ok = false;
  double m[6] = {1, 0, 0, 0, 1, 0};
  double rms = 0.0;
  std::string error;
};

// Interleaved xy (size 2n). Needs n >= 2.
GIS_EXPORT FitLineResult fit_line_2d(const std::vector<double>& xy);

// Interleaved xyz (size 3n). Needs n >= 3.
GIS_EXPORT FitPlaneResult fit_plane_3d(const std::vector<double>& xyz);

// Paired interleaved xy of equal length. Needs n >= 3.
GIS_EXPORT AffineAlignResult affine_align_2d(const std::vector<double>& src_xy,
                                             const std::vector<double>& dst_xy);

// JSON ops: input GeoJSON points → output GeoJSON LineString or JSON params.
GIS_EXPORT bool run_fit_line_op(std::string_view args_json);
GIS_EXPORT bool run_fit_plane_op(std::string_view args_json);
GIS_EXPORT bool run_affine_align_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOMETRY_FIT_H_
