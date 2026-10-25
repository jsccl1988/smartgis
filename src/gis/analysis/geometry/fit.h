// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOMETRY_FIT_H_
#define GIS_ANALYSIS_GEOMETRY_FIT_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"
#include "gis/geo/ops/geometry_traits.h"

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

// Umeyama similarity / rigid (scale fixed to 1) registration in 2D.
struct SimilarityAlignResult {
  bool ok = false;
  double scale = 1.0;
  double theta = 0.0;
  double tx = 0.0;
  double ty = 0.0;
  double m[6] = {1, 0, 0, 0, 1, 0};
  double rms = 0.0;
  std::string error;
};

// Principal axes of a 2D point set (Eigen covariance SVD).
struct PcaAxesResult {
  bool ok = false;
  double cx = 0.0;
  double cy = 0.0;
  double major_dx = 1.0;
  double major_dy = 0.0;
  double minor_dx = 0.0;
  double minor_dy = 1.0;
  double major_extent = 0.0;
  double minor_extent = 0.0;
  double eigenvalue_major = 0.0;
  double eigenvalue_minor = 0.0;
  std::string error;
};

// Interleaved xy (size 2n). Needs n >= 2.
GIS_EXPORT FitLineResult fit_line_2d(const std::vector<double>& xy);

// Interleaved xyz (size 3n). Needs n >= 3.
GIS_EXPORT FitPlaneResult fit_plane_3d(const std::vector<double>& xyz);

// OGR entry: constrained so mesh types do not instantiate.
GIS_EXPORT FitLineResult fit_line_2d_from_ogr(const OGRGeometry& geom);
GIS_EXPORT FitPlaneResult fit_plane_3d_from_ogr(const OGRGeometry& geom);

template <geo::ogr_geometry_like G>
FitLineResult fit_line_2d(const G& geom) {
  return fit_line_2d_from_ogr(geom);
}

template <geo::ogr_geometry_like G>
FitPlaneResult fit_plane_3d(const G& geom) {
  return fit_plane_3d_from_ogr(geom);
}

// Paired interleaved xy of equal length. Needs n >= 3.
GIS_EXPORT AffineAlignResult affine_align_2d(const std::vector<double>& src_xy,
                                             const std::vector<double>& dst_xy);

// Umeyama similarity (scale+rotation+translation). Needs n >= 2.
GIS_EXPORT SimilarityAlignResult similarity_align_2d(
    const std::vector<double>& src_xy,
    const std::vector<double>& dst_xy);

// Rigid align (unit scale). Needs n >= 2.
GIS_EXPORT SimilarityAlignResult rigid_align_2d(const std::vector<double>& src_xy,
                                                  const std::vector<double>& dst_xy);

// Interleaved xy (size 2n). Needs n >= 2.
GIS_EXPORT PcaAxesResult pca_axes_2d(const std::vector<double>& xy);

GIS_EXPORT PcaAxesResult pca_axes_2d_from_ogr(const OGRGeometry& geom);

template <geo::ogr_geometry_like G>
PcaAxesResult pca_axes_2d(const G& geom) {
  return pca_axes_2d_from_ogr(geom);
}

// Minimum-area rectangle aligned to PCA axes (point sets). Needs n >= 2.
GIS_EXPORT std::unique_ptr<OGRGeometry> oriented_bbox_from_points_2d(
    const std::vector<double>& xy);

GIS_EXPORT std::unique_ptr<OGRGeometry> oriented_bbox_from_ogr(const OGRGeometry& geom);

// JSON ops: input GeoJSON points → output GeoJSON LineString or JSON params.
GIS_EXPORT bool run_fit_line_op(std::string_view args_json);
GIS_EXPORT bool run_fit_plane_op(std::string_view args_json);
GIS_EXPORT bool run_affine_align_op(std::string_view args_json);
GIS_EXPORT bool run_similarity_align_op(std::string_view args_json);
GIS_EXPORT bool run_rigid_align_op(std::string_view args_json);
GIS_EXPORT bool run_pca_axes_op(std::string_view args_json);
GIS_EXPORT bool run_oriented_bbox_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOMETRY_FIT_H_
