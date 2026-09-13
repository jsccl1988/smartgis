// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/tin/tin_backend_traits.h"

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tin {
namespace detail {

struct geos_context {
	GEOSContextHandle_t handle = nullptr;

	geos_context()
	    : handle(GEOS_init_r())
	{
		if (handle != nullptr) {
			GEOSContext_setNoticeHandler_r(handle, &ignore_message);
			GEOSContext_setErrorHandler_r(handle, &ignore_message);
		}
	}

	~geos_context()
	{
		if (handle != nullptr)
			GEOS_finish_r(handle);
	}

	geos_context(const geos_context&) = delete;
	geos_context& operator=(const geos_context&) = delete;

	explicit operator bool() const { return handle != nullptr; }

private:
	static void ignore_message(const char* /*fmt*/, ...) {}
};

struct xy_key {
	std::uint64_t xbits = 0;
	std::uint64_t ybits = 0;

	bool operator==(const xy_key& other) const = default;
};

struct xy_key_hash {
	std::size_t operator()(const xy_key& key) const noexcept
	{
		return std::hash<std::uint64_t>{}(key.xbits) ^
		       (std::hash<std::uint64_t>{}(key.ybits) << 1);
	}
};

xy_key make_xy_key(double x, double y)
{
	xy_key key;
	std::memcpy(&key.xbits, &x, sizeof(x));
	std::memcpy(&key.ybits, &y, sizeof(y));
	return key;
}

class vertex_index_map {
public:
	vertex_index_map(const render::Vector3* points, int count)
	    : points_(points)
	    , count_(count)
	{
		exact_.reserve(static_cast<std::size_t>(count));
		for (int i = 0; i < count; ++i) {
			const double x = static_cast<double>(points[i].x);
			const double y = static_cast<double>(points[i].y);
			exact_.emplace(make_xy_key(x, y), i);
		}
	}

	int lookup(double x, double y) const
	{
		const auto it = exact_.find(make_xy_key(x, y));
		if (it != exact_.end())
			return it->second;

		for (int i = 0; i < count_; ++i) {
			if (SMT_EQUAL(static_cast<float>(x), points_[i].x) &&
			    SMT_EQUAL(static_cast<float>(y), points_[i].y))
				return i;
		}
		return -1;
	}

private:
	const render::Vector3* points_ = nullptr;
	int count_ = 0;
	std::unordered_map<xy_key, int, xy_key_hash> exact_;
};

void destroy_geoms(GEOSContextHandle_t ctx, std::vector<GEOSGeometry*>& geoms)
{
	for (GEOSGeometry* geom : geoms) {
		if (geom != nullptr)
			GEOSGeom_destroy_r(ctx, geom);
	}
	geoms.clear();
}

bool extract_triangle(GEOSContextHandle_t ctx,
                      const GEOSGeometry* poly,
                      const vertex_index_map& index_map,
                      base::SmtTriangle& out)
{
	if (poly == nullptr || GEOSGeomTypeId_r(ctx, poly) != GEOS_POLYGON)
		return false;

	const GEOSGeometry* ring = GEOSGetExteriorRing_r(ctx, poly);
	if (ring == nullptr)
		return false;

	const GEOSCoordSequence* coords = GEOSGeom_getCoordSeq_r(ctx, ring);
	if (coords == nullptr)
		return false;

	unsigned int size = 0;
	if (GEOSCoordSeq_getSize_r(ctx, coords, &size) == 0 || size < 3)
		return false;

	int idx[3] = {-1, -1, -1};
	double xs[3] = {};
	double ys[3] = {};
	for (unsigned int i = 0; i < 3; ++i) {
		if (GEOSCoordSeq_getXY_r(ctx, coords, i, &xs[i], &ys[i]) == 0)
			return false;
		idx[i] = index_map.lookup(xs[i], ys[i]);
		if (idx[i] < 0)
			return false;
	}

	if (idx[0] == idx[1] || idx[1] == idx[2] || idx[2] == idx[0])
		return false;

	const double cross =
	    (xs[1] - xs[0]) * (ys[2] - ys[0]) - (ys[1] - ys[0]) * (xs[2] - xs[0]);
	if (cross < 0.0)
		std::swap(idx[1], idx[2]);

	out.a = idx[0];
	out.b = idx[1];
	out.c = idx[2];
	out.bDelete = false;
	return true;
}

bool append_triangles_from(GEOSContextHandle_t ctx,
                           const GEOSGeometry* geom,
                           const vertex_index_map& index_map,
                           std::vector<base::SmtTriangle>& triangles)
{
	if (geom == nullptr)
		return false;

	const int type = GEOSGeomTypeId_r(ctx, geom);
	if (type == GEOS_POLYGON) {
		base::SmtTriangle tri;
		if (!extract_triangle(ctx, geom, index_map, tri))
			return false;
		triangles.push_back(tri);
		return true;
	}

	if (type != GEOS_GEOMETRYCOLLECTION && type != GEOS_MULTIPOLYGON)
		return false;

	const int n = GEOSGetNumGeometries_r(ctx, geom);
	if (n < 0)
		return false;

	bool mapped_any = (n == 0);
	for (int i = 0; i < n; ++i) {
		const GEOSGeometry* child = GEOSGetGeometryN_r(ctx, geom, i);
		base::SmtTriangle tri;
		if (extract_triangle(ctx, child, index_map, tri)) {
			triangles.push_back(tri);
			mapped_any = true;
		}
	}
	return mapped_any || n == 0;
}

}  // namespace detail

long tin_backend_traits<geos_backend>::triangulate(
    const render::Vector3* points,
    int count,
    std::vector<base::SmtTriangle>& triangles)
{
	triangles.clear();
	if (points == nullptr || count < 3)
		return SMT_ERR_INVALID_PARAM;

	detail::geos_context ctx;
	if (!ctx)
		return SMT_ERR_FAILURE;

	std::vector<GEOSGeometry*> sites(static_cast<std::size_t>(count), nullptr);
	for (int i = 0; i < count; ++i) {
		sites[static_cast<std::size_t>(i)] = GEOSGeom_createPointFromXY_r(
		    ctx.handle, static_cast<double>(points[i].x),
		    static_cast<double>(points[i].y));
		if (sites[static_cast<std::size_t>(i)] == nullptr) {
			detail::destroy_geoms(ctx.handle, sites);
			return SMT_ERR_FAILURE;
		}
	}

	GEOSGeometry* multipoint = GEOSGeom_createCollection_r(
	    ctx.handle, GEOS_MULTIPOINT, sites.data(),
	    static_cast<unsigned int>(count));
	if (multipoint == nullptr) {
		detail::destroy_geoms(ctx.handle, sites);
		return SMT_ERR_FAILURE;
	}

	GEOSGeometry* mesh =
	    GEOSDelaunayTriangulation_r(ctx.handle, multipoint, 0.0, 0);
	GEOSGeom_destroy_r(ctx.handle, multipoint);
	if (mesh == nullptr)
		return SMT_ERR_FAILURE;

	const detail::vertex_index_map index_map(points, count);
	const bool ok =
	    detail::append_triangles_from(ctx.handle, mesh, index_map, triangles);
	GEOSGeom_destroy_r(ctx.handle, mesh);
	return ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long tin_backend_traits<geos_backend>::triangulate_constrained(
    const render::Vector3* points,
    int count,
    std::vector<base::SmtTriangle>& triangles)
{
	triangles.clear();
	if (points == nullptr || count < 3)
		return SMT_ERR_INVALID_PARAM;

	detail::geos_context ctx;
	if (!ctx)
		return SMT_ERR_FAILURE;

	const bool closed =
	    SMT_EQUAL(points[0].x, points[count - 1].x) &&
	    SMT_EQUAL(points[0].y, points[count - 1].y);
	const unsigned int seq_size =
	    static_cast<unsigned int>(closed ? count : count + 1);

	GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx.handle, seq_size, 2);
	if (seq == nullptr)
		return SMT_ERR_FAILURE;

	for (int i = 0; i < count; ++i) {
		if (GEOSCoordSeq_setXY_r(ctx.handle, seq, static_cast<unsigned int>(i),
		                         static_cast<double>(points[i].x),
		                         static_cast<double>(points[i].y)) == 0) {
			GEOSCoordSeq_destroy_r(ctx.handle, seq);
			return SMT_ERR_FAILURE;
		}
	}
	if (!closed) {
		if (GEOSCoordSeq_setXY_r(ctx.handle, seq, seq_size - 1,
		                         static_cast<double>(points[0].x),
		                         static_cast<double>(points[0].y)) == 0) {
			GEOSCoordSeq_destroy_r(ctx.handle, seq);
			return SMT_ERR_FAILURE;
		}
	}

	GEOSGeometry* ring = GEOSGeom_createLinearRing_r(ctx.handle, seq);
	if (ring == nullptr) {
		GEOSCoordSeq_destroy_r(ctx.handle, seq);
		return SMT_ERR_FAILURE;
	}

	GEOSGeometry* poly = GEOSGeom_createPolygon_r(ctx.handle, ring, nullptr, 0);
	if (poly == nullptr) {
		GEOSGeom_destroy_r(ctx.handle, ring);
		return SMT_ERR_FAILURE;
	}

	GEOSGeometry* mesh =
	    GEOSConstrainedDelaunayTriangulation_r(ctx.handle, poly);
	GEOSGeom_destroy_r(ctx.handle, poly);
	if (mesh == nullptr)
		return SMT_ERR_FAILURE;

	const detail::vertex_index_map index_map(points, count);
	const bool ok =
	    detail::append_triangles_from(ctx.handle, mesh, index_map, triangles);
	GEOSGeom_destroy_r(ctx.handle, mesh);
	return ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

}  // namespace tin
