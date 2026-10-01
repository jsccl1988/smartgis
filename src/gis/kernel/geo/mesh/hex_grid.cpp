// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/kernel/geo/mesh/geometry.h"

#include <algorithm>
#include <cmath>

namespace geo {

HexGrid::HexGrid() = default;

HexGrid::HexGrid(int nx, int ny, int nz) {
  set_size(nx, ny, nz);
}

HexGrid::HexGrid(const HexGrid& other) {
  *this = other;
}

HexGrid& HexGrid::operator=(const HexGrid& other) {
  if (this == &other) {
    return *this;
  }
  nx_ = other.nx_;
  ny_ = other.ny_;
  nz_ = other.nz_;
  nodes_ = other.nodes_;
  return *this;
}

HexGrid::~HexGrid() {
  clear();
}

HexGrid* HexGrid::clone() const {
  return new HexGrid(*this);
}

void HexGrid::clear() {
  nodes_.clear();
  nx_ = 0;
  ny_ = 0;
  nz_ = 0;
}

bool HexGrid::is_empty() const {
  return nx_ < 1 || ny_ < 1 || nz_ < 1 || nodes_.empty();
}

void HexGrid::set_size(int nx, int ny, int nz) {
  clear();
  if (nx < 1 || ny < 1 || nz < 1) {
    return;
  }
  nx_ = nx;
  ny_ = ny;
  nz_ = nz;
  nodes_.assign(static_cast<size_t>(nx_ * ny_ * nz_), Raw3DPoint());
}

void HexGrid::resize(int nx, int ny, int nz) {
  if (nx == nx_ && ny == ny_ && nz == nz_) {
    return;
  }
  set_size(nx, ny, nz);
}

void HexGrid::get_size(int& nx, int& ny, int& nz) const {
  nx = nx_;
  ny = ny_;
  nz = nz_;
}

int HexGrid::index_of(int i, int j, int k) const {
  if (i < 0 || j < 0 || k < 0 || i >= nx_ || j >= ny_ || k >= nz_) {
    return -1;
  }
  return k * ny_ * nx_ + j * nx_ + i;
}

Raw3DPoint HexGrid::node(int i, int j, int k) const {
  const int idx = index_of(i, j, k);
  if (idx < 0) {
    return Raw3DPoint();
  }
  return nodes_[static_cast<size_t>(idx)];
}

void HexGrid::set_node(int i, int j, int k, const Raw3DPoint& p) {
  const int idx = index_of(i, j, k);
  if (idx < 0) {
    return;
  }
  nodes_[static_cast<size_t>(idx)] = p;
}

void HexGrid::get_envelope(OGREnvelope3D* env) const {
  if (env == nullptr || is_empty()) {
    return;
  }
  env->MinX = env->MaxX = nodes_[0].x;
  env->MinY = env->MaxY = nodes_[0].y;
  env->MinZ = env->MaxZ = nodes_[0].z;
  for (const Raw3DPoint& p : nodes_) {
    env->MinX = (std::min)(env->MinX, p.x);
    env->MaxX = (std::max)(env->MaxX, p.x);
    env->MinY = (std::min)(env->MinY, p.y);
    env->MaxY = (std::max)(env->MaxY, p.y);
    env->MinZ = (std::min)(env->MinZ, p.z);
    env->MaxZ = (std::max)(env->MaxZ, p.z);
  }
}

bool HexGrid::equals(const HexGrid* other) const {
  if (other == this) {
    return true;
  }
  if (other == nullptr) {
    return false;
  }
  if (nx_ != other->nx_ || ny_ != other->ny_ || nz_ != other->nz_) {
    return false;
  }
  if (is_empty() && other->is_empty()) {
    return true;
  }
  for (size_t n = 0; n < nodes_.size(); ++n) {
    const Raw3DPoint& a = nodes_[n];
    const Raw3DPoint& b = other->nodes_[n];
    if (a.x != b.x || a.y != b.y || a.z != b.z) {
      return false;
    }
  }
  return true;
}

}  // namespace geo
