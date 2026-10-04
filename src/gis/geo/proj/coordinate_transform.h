// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_PROJ_COORDINATE_TRANSFORM_H_
#define GIS_GEO_PROJ_COORDINATE_TRANSFORM_H_

#include "gis/gis_export.h"

#include <string_view>

namespace geo {

// RAII CRS-to-CRS pipeline over the shipped PROJ 9 C API (`proj.h` is
// included only in the .cc). Strings are EPSG / WKT / PROJ.
class GIS_EXPORT CoordinateTransform {
 public:
  CoordinateTransform(std::string_view source_crs,
                      std::string_view target_crs);
  CoordinateTransform(CoordinateTransform&&) noexcept;
  CoordinateTransform& operator=(CoordinateTransform&&) noexcept;
  ~CoordinateTransform();

  CoordinateTransform(const CoordinateTransform&) = delete;
  CoordinateTransform& operator=(const CoordinateTransform&) = delete;

  bool is_valid() const;

  // Updates `x` and `y` in place. Returns false on invalid pipeline or
  // a non-finite PROJ result.
  bool transform_xy(double& x, double& y) const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

// One-shot helper. Prefer CoordinateTransform when transforming many points.
GIS_EXPORT bool transform_xy(std::string_view source_crs,
                             std::string_view target_crs,
                             double& x,
                             double& y);

}  // namespace geo

#endif  // GIS_GEO_PROJ_COORDINATE_TRANSFORM_H_
