// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_TIN_TIN_H_
#define ALGORITHM_TIN_TIN_H_

#include "algorithm/geo/geometry.h"
#include "render/math/math.h"

#include <vector>

#if defined(TIN_EXPORTS)
#define TIN_EXPORT_API __declspec(dllexport)
#else
#define TIN_EXPORT_API __declspec(dllimport)
#endif

// Exported Delaunay entry points. Div/Inc names share tin::tin_backend_traits
// (GEOS geos_c). Surface3d is a thin Tin derived type (Z on nodes).

TIN_EXPORT_API long create_delaunay_tin_div(geo::Tin* tin,
                                            const render::Vector3* points,
                                            int count);
TIN_EXPORT_API long create_delaunay_tin_div(geo::Tin* tin,
                                            render::Vector3* points,
                                            int count);
TIN_EXPORT_API long create_delaunay_tin_div(
    geo::Tin* tin,
    const std::vector<render::Vector3>& points);

TIN_EXPORT_API long create_delaunay_tin_inc(geo::Tin* tin,
                                            const render::Vector3* points,
                                            int count);
TIN_EXPORT_API long create_delaunay_tin_inc(geo::Tin* tin,
                                            render::Vector3* points,
                                            int count);
TIN_EXPORT_API long create_delaunay_tin_inc(
    geo::Tin* tin,
    const std::vector<render::Vector3>& points);

TIN_EXPORT_API long divide_polygon_into_tri_mesh(
    std::vector<base::SmtTriangle>& triangles,
    base::dbfPoint* points,
    int n_point);

#if !defined(TIN_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "algorithm_d.lib")
#else
#pragma comment(lib, "algorithm.lib")
#endif
#endif

#endif  // ALGORITHM_TIN_TIN_H_
