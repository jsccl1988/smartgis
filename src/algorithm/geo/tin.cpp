// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/geo/geometry.h"

using namespace base;

namespace geo {
namespace {

bool face_in_range(const SmtTriangle& tri, int n_points) {
  return tri.a >= 0 && tri.b >= 0 && tri.c >= 0 && tri.a < n_points &&
         tri.b < n_points && tri.c < n_points;
}

}  // namespace

Tin::Tin() = default;

Tin::Tin(const Tin& other)
    : mesh_(other.mesh_),
      nodes_(other.nodes_),
      faces_(other.faces_),
      deleted_(other.deleted_) {}

Tin& Tin::operator=(const Tin& other) {
  if (this == &other) {
    return *this;
  }
  mesh_ = other.mesh_;
  nodes_ = other.nodes_;
  faces_ = other.faces_;
  deleted_ = other.deleted_;
  return *this;
}

Tin::~Tin() { clear(); }

Tin* Tin::clone() const { return new Tin(*this); }

void Tin::clear() {
  mesh_.empty();
  nodes_.clear();
  faces_.clear();
  deleted_.clear();
}

bool Tin::is_empty() const { return nodes_.empty(); }

void Tin::get_envelope(Envelope* psEnvelope) const {
  if (psEnvelope == nullptr || nodes_.empty()) {
    return;
  }
  if (mesh_.getNumGeometries() > 0) {
    copy_envelope(mesh_, psEnvelope);
    return;
  }
  psEnvelope->MaxX = nodes_[0].x;
  psEnvelope->MaxY = nodes_[0].y;
  psEnvelope->MinX = nodes_[0].x;
  psEnvelope->MinY = nodes_[0].y;
  for (const dbf3DPoint& p : nodes_) {
    psEnvelope->merge(p.x, p.y);
  }
}

void Tin::get_envelope(OGREnvelope3D* env) const {
  if (env == nullptr) {
    return;
  }
  env->MinX = env->MinY = env->MinZ = 0;
  env->MaxX = env->MaxY = env->MaxZ = 0;
  if (nodes_.empty()) {
    return;
  }
  if (mesh_.getNumGeometries() > 0) {
    mesh_.getEnvelope(env);
    return;
  }
  env->MinX = env->MaxX = nodes_[0].x;
  env->MinY = env->MaxY = nodes_[0].y;
  env->MinZ = env->MaxZ = nodes_[0].z;
  for (size_t i = 1; i < nodes_.size(); ++i) {
    if (nodes_[i].x < env->MinX) {
      env->MinX = nodes_[i].x;
    }
    if (nodes_[i].y < env->MinY) {
      env->MinY = nodes_[i].y;
    }
    if (nodes_[i].z < env->MinZ) {
      env->MinZ = nodes_[i].z;
    }
    if (nodes_[i].x > env->MaxX) {
      env->MaxX = nodes_[i].x;
    }
    if (nodes_[i].y > env->MaxY) {
      env->MaxY = nodes_[i].y;
    }
    if (nodes_[i].z > env->MaxZ) {
      env->MaxZ = nodes_[i].z;
    }
  }
}

SmtTriangle Tin::get_triangle(int iIndex) const {
  if (iIndex < 0 || iIndex >= get_triangle_count()) {
    return SmtTriangle();
  }
  SmtTriangle tri;
  tri.a = faces_[static_cast<size_t>(iIndex)].a;
  tri.b = faces_[static_cast<size_t>(iIndex)].b;
  tri.c = faces_[static_cast<size_t>(iIndex)].c;
  tri.bDelete = deleted_[static_cast<size_t>(iIndex)] != 0;
  return tri;
}

OGRPoint Tin::get_point(int iIndex) const {
  OGRPoint rawPoint;
  if (iIndex >= 0 && iIndex < get_point_count()) {
    const dbf3DPoint& p = nodes_[static_cast<size_t>(iIndex)];
    rawPoint.setX(p.x);
    rawPoint.setY(p.y);
    rawPoint.setZ(p.z);
  }
  return rawPoint;
}

int Tin::add_point(OGRPoint* poPoint) {
  if (poPoint == nullptr) {
    return SMT_ERR_FAILURE;
  }
  nodes_.push_back(
      dbf3DPoint(poPoint->getX(), poPoint->getY(), poPoint->getZ()));
  return SMT_ERR_NONE;
}

int Tin::add_point_collection(OGRPoint* poPoint, int nPoints) {
  if (poPoint == nullptr || nPoints < 1) {
    return SMT_ERR_FAILURE;
  }
  nodes_.reserve(nodes_.size() + static_cast<size_t>(nPoints));
  for (int i = 0; i < nPoints; ++i) {
    nodes_.push_back(
        dbf3DPoint(poPoint[i].getX(), poPoint[i].getY(), poPoint[i].getZ()));
  }
  return SMT_ERR_NONE;
}

int Tin::add_point_collection(dbfPoint* pPoints, int nPoints) {
  if (pPoints == nullptr || nPoints < 1) {
    return SMT_ERR_FAILURE;
  }
  nodes_.reserve(nodes_.size() + static_cast<size_t>(nPoints));
  for (int i = 0; i < nPoints; ++i) {
    nodes_.push_back(dbf3DPoint(pPoints[i].x, pPoints[i].y, 0));
  }
  return SMT_ERR_NONE;
}

int Tin::add_point_collection(dbf3DPoint* pPoints, int nPoints) {
  if (pPoints == nullptr || nPoints < 1) {
    return SMT_ERR_FAILURE;
  }
  nodes_.insert(nodes_.end(), pPoints, pPoints + nPoints);
  return SMT_ERR_NONE;
}

int Tin::add_ogr_face(const SmtTriangle& tri) {
  if (!face_in_range(tri, get_point_count())) {
    return SMT_ERR_FAILURE;
  }
  const dbf3DPoint& a = nodes_[static_cast<size_t>(tri.a)];
  const dbf3DPoint& b = nodes_[static_cast<size_t>(tri.b)];
  const dbf3DPoint& c = nodes_[static_cast<size_t>(tri.c)];
  const OGRPoint pa(a.x, a.y, a.z);
  const OGRPoint pb(b.x, b.y, b.z);
  const OGRPoint pc(c.x, c.y, c.z);
  const OGRTriangle patch(pa, pb, pc);
  if (mesh_.addGeometry(&patch) != OGRERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  Face face;
  face.a = static_cast<int>(tri.a);
  face.b = static_cast<int>(tri.b);
  face.c = static_cast<int>(tri.c);
  faces_.push_back(face);
  deleted_.push_back(tri.bDelete ? 1 : 0);
  return SMT_ERR_NONE;
}

int Tin::add_triangle(SmtTriangle* poNewTri) {
  if (poNewTri == nullptr) {
    return SMT_ERR_FAILURE;
  }
  return add_ogr_face(*poNewTri);
}

int Tin::add_triangle_collection(SmtTriangle* poTris, int nTris) {
  if (poTris == nullptr || nTris < 1) {
    return SMT_ERR_FAILURE;
  }
  for (int i = 0; i < nTris; ++i) {
    if (add_ogr_face(poTris[i]) != SMT_ERR_NONE) {
      return SMT_ERR_FAILURE;
    }
  }
  return SMT_ERR_NONE;
}

int Tin::remove_triangle(int iIndex) {
  const int n = get_triangle_count();
  if (iIndex < -1 || iIndex >= n) {
    return SMT_ERR_FAILURE;
  }
  if (iIndex == -1) {
    for (char& flag : deleted_) {
      flag = 1;
    }
    return SMT_ERR_NONE;
  }
  deleted_[static_cast<size_t>(iIndex)] = 1;
  return SMT_ERR_NONE;
}

long Tin::copy_to_tin(Tin* tin) const {
  if (tin == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  const int n_points = get_point_count();
  const int n_tris = get_triangle_count();
  if (n_points > 0) {
    std::vector<OGRPoint> point_buf(static_cast<size_t>(n_points));
    for (int i = 0; i < n_points; ++i) {
      const OGRPoint o3d = get_point(i);
      point_buf[static_cast<size_t>(i)].setX(o3d.getX());
      point_buf[static_cast<size_t>(i)].setY(o3d.getY());
    }
    tin->add_point_collection(point_buf.data(), n_points);
  }
  if (n_tris > 0) {
    std::vector<SmtTriangle> tri_buf(static_cast<size_t>(n_tris));
    for (int i = 0; i < n_tris; ++i) {
      tri_buf[static_cast<size_t>(i)] = get_triangle(i);
    }
    tin->add_triangle_collection(tri_buf.data(), n_tris);
  }
  return SMT_ERR_NONE;
}

}  // namespace geo
