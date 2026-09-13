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

  std::vector<Vector3> vertices(static_cast<std::size_t>(nPoint));
  for (int i = 0; i < nPoint; ++i) {
    vertices[static_cast<std::size_t>(i)].x = static_cast<float>(pPoints[i].x);
    vertices[static_cast<std::size_t>(i)].y = static_cast<float>(pPoints[i].y);
    vertices[static_cast<std::size_t>(i)].z = 0.f;
  }

  std::vector<SmtTriangle> triangles;
  const long rc =
      tin::tin_backend_traits<tin::default_backend>::triangulate_constrained(
          vertices.data(), nPoint, triangles);
  if (rc != SMT_ERR_NONE) {
    return rc;
  }

  OGRLinearRing ring;
  std::vector<OGRRawPoint> raw(static_cast<std::size_t>(nPoint));
  for (int i = 0; i < nPoint; ++i) {
    raw[static_cast<std::size_t>(i)].x = pPoints[i].x;
    raw[static_cast<std::size_t>(i)].y = pPoints[i].y;
  }
  ring.setPoints(nPoint, raw.data());
  ring.closeRings();
  OGRPolygon poly;
  poly.addRing(&ring);

  for (const SmtTriangle& tri : triangles) {
    const Vector3& a = vertices[static_cast<std::size_t>(tri.a)];
    const Vector3& b = vertices[static_cast<std::size_t>(tri.b)];
    const Vector3& c = vertices[static_cast<std::size_t>(tri.c)];
    OGRPoint center((static_cast<double>(a.x) + b.x + c.x) / 3.0,
                    (static_cast<double>(a.y) + b.y + c.y) / 3.0);
    if (poly.Contains(&center)) {
      trilist.push_back(tri);
    }
  }

  return SMT_ERR_NONE;
}
