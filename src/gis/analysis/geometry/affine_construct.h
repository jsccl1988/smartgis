// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOMETRY_AFFINE_CONSTRUCT_H_
#define GIS_ANALYSIS_GEOMETRY_AFFINE_CONSTRUCT_H_

#include "gis/gis_export.h"

class OGRGeometry;

namespace gis {
namespace detail {

// In-place 2D affine: x' = m0*x + m1*y + m2; y' = m3*x + m4*y + m5.
GIS_EXPORT bool apply_affine_2d_inplace(OGRGeometry* geom, const double m[6]);

GIS_EXPORT bool affine_translate_2d(OGRGeometry* geom, double dx, double dy);

// Counter-clockwise rotation in radians about (origin_x, origin_y).
GIS_EXPORT bool affine_rotate_2d(OGRGeometry* geom,
                                 double angle_rad,
                                 double origin_x,
                                 double origin_y);

GIS_EXPORT bool affine_scale_2d(OGRGeometry* geom,
                                double scale_x,
                                double scale_y,
                                double origin_x,
                                double origin_y);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOMETRY_AFFINE_CONSTRUCT_H_
