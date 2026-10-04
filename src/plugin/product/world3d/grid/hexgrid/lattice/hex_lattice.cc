// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/hexgrid/lattice/hex_lattice.h"

namespace plugin {
namespace detail {
namespace {

void fill_empty_nodes(OGRMultiPoint* nodes, int count) {
  if (nodes == nullptr) {
    return;
  }
  nodes->empty();
  for (int i = 0; i < count; ++i) {
    OGRPoint pt(0, 0, 0);
    pt.setCoordinateDimension(3);
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

int lattice_index(int nx, int ny, int nz, int i, int j, int k) {
  if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) {
    return -1;
  }
  return k * ny * nx + j * nx + i;
}

bool read_point(const OGRMultiPoint& nodes,
                int nx,
                int ny,
                int nz,
                int i,
                int j,
                int k,
                double* x,
                double* y,
                double* z) {
  const int idx = lattice_index(nx, ny, nz, i, j, k);
  const OGRPoint* pt = point_at(nodes, idx);
  if (pt == nullptr || x == nullptr || y == nullptr || z == nullptr) {
    return false;
  }
  *x = pt->getX();
  *y = pt->getY();
  *z = pt->getZ();
  return true;
}

}  // namespace

int HexLattice::hex_index(int i, int j, int k) const {
  return lattice_index(nx, ny, nz, i, j, k);
}

void HexLattice::hex_resize(int nx_in, int ny_in, int nz_in) {
  if (nx_in == nx && ny_in == ny && nz_in == nz && !is_empty()) {
    return;
  }
  nodes.empty();
  nx = 0;
  ny = 0;
  nz = 0;
  if (nx_in < 1 || ny_in < 1 || nz_in < 1) {
    return;
  }
  nx = nx_in;
  ny = ny_in;
  nz = nz_in;
  fill_empty_nodes(&nodes, nx * ny * nz);
}

bool HexLattice::hex_point(int i, int j, int k, double* x, double* y,
                           double* z) const {
  return read_point(nodes, nx, ny, nz, i, j, k, x, y, z);
}

bool hex_point(const OGRMultiPoint& nodes, int nx, int ny, int nz, int i,
               int j, int k, double* x, double* y, double* z) {
  return read_point(nodes, nx, ny, nz, i, j, k, x, y, z);
}

void HexLattice::hex_set_point(int i, int j, int k, double x, double y,
                               double z) {
  OGRPoint* pt = point_at(&nodes, hex_index(i, j, k));
  if (pt == nullptr) {
    return;
  }
  pt->setX(x);
  pt->setY(y);
  pt->setZ(z);
  pt->setCoordinateDimension(3);
}

}  // namespace detail
}  // namespace plugin
