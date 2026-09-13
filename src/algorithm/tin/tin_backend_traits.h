// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_TIN_TIN_BACKEND_TRAITS_H_
#define ALGORITHM_TIN_TIN_BACKEND_TRAITS_H_

#include "algorithm/geo/geometry.h"
#include "render/math/math.h"

#include <concepts>
#include <vector>

namespace tin {

// GEOS 3.13 Delaunay via geos_c from the shared third_party install
// prefix. Both Div and Inc export names dispatch here; they are the
// same OSS algorithm.
struct geos_backend {};

using default_backend = geos_backend;

// Backend contract: triangulate input vertices (XY Delaunay, Z unused).
// Triangle indices refer to the input array order.
template <typename Backend>
struct tin_backend_traits;

template <>
struct tin_backend_traits<geos_backend> {
	static long triangulate(const render::Vector3* points,
	                        int count,
	                        std::vector<base::SmtTriangle>& triangles);
	static long triangulate_constrained(
	    const render::Vector3* points,
	    int count,
	    std::vector<base::SmtTriangle>& triangles);
};

template <typename Backend>
concept delaunay_backend = requires(const render::Vector3* points,
                                    int count,
                                    std::vector<base::SmtTriangle>& triangles) {
	{
		tin_backend_traits<Backend>::triangulate(points, count, triangles)
	} -> std::same_as<long>;
};

namespace detail {

template <delaunay_backend Backend>
long triangulate_or_empty(const render::Vector3* points,
                          int count,
                          std::vector<base::SmtTriangle>& triangles)
{
	triangles.clear();
	if (count < 3)
		return SMT_ERR_NONE;
	return tin_backend_traits<Backend>::triangulate(points, count, triangles);
}

}  // namespace detail

template <delaunay_backend Backend = default_backend>
long fill_tin(geo::Tin* tin,
              const render::Vector3* points,
              int count)
{
	if (tin == nullptr || points == nullptr || count < 1)
		return SMT_ERR_INVALID_PARAM;

	std::vector<base::SmtTriangle> triangles;
	const long rc = detail::triangulate_or_empty<Backend>(points, count, triangles);
	if (rc != SMT_ERR_NONE)
		return rc;

	std::vector<OGRPoint> point_buf(static_cast<std::size_t>(count));
	for (int i = 0; i < count; ++i) {
		point_buf[static_cast<std::size_t>(i)].setX(points[i].x);
		point_buf[static_cast<std::size_t>(i)].setY(points[i].y);
		point_buf[static_cast<std::size_t>(i)].setZ(points[i].z);
	}
	tin->add_point_collection(point_buf.data(), count);

	if (!triangles.empty())
		tin->add_triangle_collection(triangles.data(),
		                           static_cast<int>(triangles.size()));

	return SMT_ERR_NONE;
}

template <delaunay_backend Backend = default_backend>
long fill_surface(geo::Surface3d* surf,
                  const render::Vector3* points,
                  int count)
{
	if (surf == nullptr || points == nullptr || count < 1)
		return SMT_ERR_INVALID_PARAM;

	std::vector<base::SmtTriangle> triangles;
	const long rc = detail::triangulate_or_empty<Backend>(points, count, triangles);
	if (rc != SMT_ERR_NONE)
		return rc;

	std::vector<OGRPoint> point_buf(static_cast<std::size_t>(count));
	for (int i = 0; i < count; ++i) {
		point_buf[static_cast<std::size_t>(i)].setX(points[i].x);
		point_buf[static_cast<std::size_t>(i)].setY(points[i].y);
		point_buf[static_cast<std::size_t>(i)].setZ(points[i].z);
	}
	surf->add_point_collection(point_buf.data(), count);

	if (!triangles.empty())
		surf->add_triangle_collection(triangles.data(),
		                            static_cast<int>(triangles.size()));

	return SMT_ERR_NONE;
}

}  // namespace tin

#endif  // ALGORITHM_TIN_TIN_BACKEND_TRAITS_H_
