// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/tin/tin.h"

#include "algorithm/tin/tin_backend_traits.h"

using namespace base;
using namespace geo;
using namespace render;

namespace {

long fill_tin_from_span(Tin* tin, const Vector3* points, int count) {
  return tin::fill_tin<tin::default_backend>(tin, points, count);
}

}  // namespace

long create_delaunay_tin_div(Tin* pTin,
                             const Vector3* pVector3Ds,
                             int nCount) {
  return fill_tin_from_span(pTin, pVector3Ds, nCount);
}

long create_delaunay_tin_div(Tin* pTin, Vector3* pVector3Ds, int nCount) {
  return create_delaunay_tin_div(pTin, static_cast<const Vector3*>(pVector3Ds),
                                 nCount);
}

long create_delaunay_tin_div(Tin* pTin, const vector<Vector3>& vVector3Ds) {
  if (vVector3Ds.empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  return fill_tin_from_span(pTin, vVector3Ds.data(),
                            static_cast<int>(vVector3Ds.size()));
}

long create_delaunay_tin_inc(Tin* pTin,
                             const Vector3* pVector3Ds,
                             int nCount) {
  return create_delaunay_tin_div(pTin, pVector3Ds, nCount);
}

long create_delaunay_tin_inc(Tin* pTin, Vector3* pVector3Ds, int nCount) {
  return create_delaunay_tin_div(pTin, static_cast<const Vector3*>(pVector3Ds),
                                 nCount);
}

long create_delaunay_tin_inc(Tin* pTin, const vector<Vector3>& vVector3Ds) {
  return create_delaunay_tin_div(pTin, vVector3Ds);
}

long divide_polygon_into_tri_mesh(vector<SmtTriangle>& trilist,
                                  dbfPoint* pPoints,
                                  int nPoint) {
  if (pPoints == nullptr || nPoint < 3) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Dense OGR rings (coastal prefectures) hang the constrained TIN. Cap
  // vertices first, then fan; PIP drops exterior slivers on concave coasts.
  constexpr int kMaxTessVerts = 64;
  std::vector<dbfPoint> decimated;
  const dbfPoint* pts = pPoints;
  int use_n = nPoint;
  if (nPoint > kMaxTessVerts) {
    decimated.resize(static_cast<std::size_t>(kMaxTessVerts));
    const int step = (nPoint + kMaxTessVerts - 2) / (kMaxTessVerts - 1);
    int out = 0;
    for (int i = 0; i < nPoint && out < kMaxTessVerts - 1; i += step) {
      decimated[static_cast<std::size_t>(out++)] = pPoints[i];
    }
    decimated[static_cast<std::size_t>(out++)] = pPoints[nPoint - 1];
    use_n = out;
    decimated.resize(static_cast<std::size_t>(use_n));
    pts = decimated.data();
  }

  std::vector<Vector3> vertices(static_cast<std::size_t>(use_n));
  for (int i = 0; i < use_n; ++i) {
    vertices[static_cast<std::size_t>(i)].x = static_cast<float>(pts[i].x);
    vertices[static_cast<std::size_t>(i)].y = static_cast<float>(pts[i].y);
    vertices[static_cast<std::size_t>(i)].z = 0.f;
  }

  std::vector<SmtTriangle> triangles;
  triangles.reserve(static_cast<std::size_t>(use_n));
  for (int i = 1; i + 1 < use_n; ++i) {
    SmtTriangle t;
    t.a = 0;
    t.b = i;
    t.c = i + 1;
    triangles.push_back(t);
  }

  // OGRPolygon::Contains needs GEOS; GDAL in this tree is often built
  // without it ("GEOS support not enabled") and would drop every triangle.
  auto point_in_ring = [pts, use_n](double x, double y) {
    bool inside = false;
    for (int i = 0, j = use_n - 1; i < use_n; j = i++) {
      const double yi = pts[i].y;
      const double yj = pts[j].y;
      const double xi = pts[i].x;
      const double xj = pts[j].x;
      if ((yi > y) != (yj > y)) {
        const double x_cross =
            (xj - xi) * (y - yi) / ((yj - yi) + 1e-30) + xi;
        if (x < x_cross) {
          inside = !inside;
        }
      }
    }
    return inside;
  };

  for (const SmtTriangle& tri : triangles) {
    if (tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= use_n ||
        tri.b >= use_n || tri.c >= use_n) {
      continue;
    }
    const Vector3& a = vertices[static_cast<std::size_t>(tri.a)];
    const Vector3& b = vertices[static_cast<std::size_t>(tri.b)];
    const Vector3& c = vertices[static_cast<std::size_t>(tri.c)];
    const double cx =
        (static_cast<double>(a.x) + b.x + c.x) / 3.0;
    const double cy =
        (static_cast<double>(a.y) + b.y + c.y) / 3.0;
    if (point_in_ring(cx, cy)) {
      trilist.push_back(tri);
    }
  }

  return SMT_ERR_NONE;
}
