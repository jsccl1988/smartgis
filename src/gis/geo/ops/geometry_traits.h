// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_GEOMETRY_TRAITS_H_
#define GIS_GEO_OPS_GEOMETRY_TRAITS_H_

#include "gis/envelope.h"
#include "ogr_geometry.h"

#include <concepts>
#include <type_traits>

namespace geo {

inline constexpr int k_ok = 0;
inline constexpr int k_fail = 1;

// Copy OGR 2D MBR into the product Envelope type.
inline void fill_envelope(const OGRGeometry& geom, gis::Envelope* envelope) {
  if (envelope == nullptr) {
    return;
  }
  OGREnvelope env;
  geom.getEnvelope(&env);
  envelope->MinX = env.MinX;
  envelope->MinY = env.MinY;
  envelope->MaxX = env.MaxX;
  envelope->MaxY = env.MaxY;
}

inline void fill_envelope3d(const OGRGeometry& geom, OGREnvelope3D* env) {
  if (env == nullptr) {
    return;
  }
  geom.getEnvelope(env);
}

// Instance OGC coordinateDimension (2 or 3) for product OGR types.
// Compile-time dimension lives on vectors, not on Geometry.
template <typename G>
struct geometry_traits;

template <typename G>
  requires std::is_base_of_v<OGRGeometry, G>
struct geometry_traits<G> {
  using coordinate_type = double;

  static int coordinate_dimension(const G& g) {
    return g.getCoordinateDimension();
  }
};

template <typename G>
concept geometry_like = requires(const G& g) {
  typename geometry_traits<G>::coordinate_type;
  geometry_traits<G>::coordinate_dimension(g);
};

// OGC operators (buffer, OGR predicates). OGC TIN is
// OGRTriangulatedSurface (ogr_geometry_like). Structured XY lattices are
// plugin OrthoLattice, not OGRGeometry.
template <typename G>
concept ogr_geometry_like =
    geometry_like<G> && std::derived_from<G, OGRGeometry>;

}  // namespace geo

#endif  // GIS_GEO_OPS_GEOMETRY_TRAITS_H_
