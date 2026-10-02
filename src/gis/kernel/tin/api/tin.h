// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_TIN_TIN_H_
#define ALGORITHM_TIN_TIN_H_


#include "gis/gis_export.h"
#include "gis/kernel/geo/mesh/geometry.h"
#include "base/math/math.h"

#include <vector>

// Exported Delaunay entry points. Div/Inc names share tin::tin_backend_traits
// (GEOS geos_c). Surface3d is a thin Tin derived type (Z on nodes).

// Use base::Vector3 (not render:: alias): MSVC dllimport mangles aliases as
// the underlying type, so render:: in the header / base:: in the .dll breaks
// LNK2019 for world3d tin_loader.
GIS_EXPORT long create_delaunay_tin_div(geo::Tin* tin,
                                            const base::Vector3* points,
                                            int count);
GIS_EXPORT long create_delaunay_tin_div(geo::Tin* tin,
                                            base::Vector3* points,
                                            int count);
GIS_EXPORT long create_delaunay_tin_div(
    geo::Tin* tin,
    const std::vector<base::Vector3>& points);

GIS_EXPORT long create_delaunay_tin_inc(geo::Tin* tin,
                                            const base::Vector3* points,
                                            int count);
GIS_EXPORT long create_delaunay_tin_inc(geo::Tin* tin,
                                            base::Vector3* points,
                                            int count);
GIS_EXPORT long create_delaunay_tin_inc(
    geo::Tin* tin,
    const std::vector<base::Vector3>& points);

GIS_EXPORT long divide_polygon_into_tri_mesh(
    std::vector<base::SmtTriangle>& triangles,
    base::dbfPoint* points,
    int n_point);


#endif  // ALGORITHM_TIN_TIN_H_
