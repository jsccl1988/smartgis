// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/orthogrid/lattice/ortho_lattice.h"

namespace plugin {
namespace detail {
namespace {

void fill_empty_nodes(OGRMultiPoint* nodes, int count) {
  if (nodes == nullptr) {
    return;
  }
  nodes->empty();
  for (int i = 0; i < count; ++i) {
    const OGRPoint pt(0, 0);
    nodes->addGeometry(&pt);
  }
}

OGRPoint* point_at(OGRMultiPoint* nodes, int idx) {
  if (nodes == nullptr || idx < 0 || idx >= nodes->getNumGeometries()) {
    return nullptr;
  }
  return nodes->getGeometryRef(idx);
}

const OGRPoint* point_at(const OGRMultiPoint& nodes, int idx) {
  if (idx < 0 || idx >= nodes.getNumGeometries()) {
    return nullptr;
  }
  return const_cast<OGRMultiPoint&>(nodes).getGeometryRef(idx);
}

int lattice_index(int nx, int ny, int i, int j) {
  if (i < 0 || j < 0 || i >= nx || j >= ny) {
    return -1;
  }
  return j * nx + i;
}

}  // namespace

int OrthoLattice::ortho_index(int i, int j) const {
  return lattice_index(nx, ny, i, j);
}

void OrthoLattice::ortho_resize(int nx_in, int ny_in) {
  if (nx_in == nx && ny_in == ny && !is_empty()) {
    return;
  }
  nodes.empty();
  nx = 0;
  ny = 0;
  if (nx_in < 1 || ny_in < 1) {
    return;
  }
  nx = nx_in;
  ny = ny_in;
  fill_empty_nodes(&nodes, nx * ny);
}

bool OrthoLattice::ortho_point(int i, int j, double* x, double* y) const {
  const OGRPoint* pt = point_at(nodes, ortho_index(i, j));
  if (pt == nullptr || x == nullptr || y == nullptr) {
    return false;
  }
  *x = pt->getX();
  *y = pt->getY();
  return true;
}

void OrthoLattice::ortho_set_point(int i, int j, double x, double y) {
  OGRPoint* pt = point_at(&nodes, ortho_index(i, j));
  if (pt == nullptr) {
    return;
  }
  pt->setX(x);
  pt->setY(y);
}

}  // namespace detail
}  // namespace plugin
