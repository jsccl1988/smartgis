// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_VISTA_COORD_H_
#define SMT_LEGACY_GIS_VISTA_COORD_H_

#include "base/math/aabb.h"
#include "gis/gis_export.h"
#include "gis/vista/world/world.h"

namespace render {

// Convert leftover Y-up corner (X=-lon, Y=elev, Z=lat) into GIS World envelope
// (min_x/min_y/min_z)–(max_x/max_y/max_z) where horizontal is lon/lat and
// vertical elev lands in Z. Pure logic — no SmtScene / render device.
GIS_EXPORT void leftover_yup_to_gis(double x0, double elev0, double lat0,
                                    double x1, double elev1, double lat1,
                                    double* min_x, double* min_y, double* min_z,
                                    double* max_x, double* max_y, double* max_z);

GIS_EXPORT void leftover_aabb_to_gis(const Aabb& aabb, double* min_x,
                                     double* min_y, double* min_z, double* max_x,
                                     double* max_y, double* max_z);

// Attach one GIS AABB as an empty spatial placeholder (octree mirror seam).
GIS_EXPORT gis::Node* attach_gis_aabb(gis::World* world, const char* name,
                                      double min_x, double min_y, double min_z,
                                      double max_x, double max_y, double max_z);

}  // namespace render

#endif  // SMT_LEGACY_GIS_VISTA_COORD_H_
