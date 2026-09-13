// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/geo/geometry.h"

using namespace base;

namespace geo {

Grid::Grid() : m_nRow(0), m_nCol(0) {}

Grid::Grid(int nRow, int nCol) : m_nRow(0), m_nCol(0) {
  set_size(nRow, nCol);
}

Grid::Grid(const Grid& other) : m_nRow(0), m_nCol(0) { *this = other; }

Grid& Grid::operator=(const Grid& other) {
  if (this == &other) {
    return *this;
  }
  nodes_ = other.nodes_;
  m_nRow = other.m_nRow;
  m_nCol = other.m_nCol;
  return *this;
}

Grid::~Grid() { clear(); }

Grid* Grid::clone() const { return new Grid(*this); }

void Grid::clear() {
  nodes_.empty();
  m_nRow = 0;
  m_nCol = 0;
}

bool Grid::is_empty() const {
  return m_nRow < 1 || m_nCol < 1 || nodes_.IsEmpty();
}

void Grid::get_envelope(Envelope* psEnvelope) const {
  if (psEnvelope == nullptr || is_empty()) {
    return;
  }
  copy_envelope(nodes_, psEnvelope);
}

bool Grid::equals(const Grid* poOGrid) const {
  if (poOGrid == this) {
    return true;
  }
  if (poOGrid == nullptr) {
    return false;
  }
  int nRow = 0;
  int nCol = 0;
  poOGrid->get_size(nRow, nCol);
  if (nRow != m_nRow || nCol != m_nCol) {
    return false;
  }
  if (is_empty() && poOGrid->is_empty()) {
    return true;
  }
  for (int i = 0; i < m_nRow; ++i) {
    for (int j = 0; j < m_nCol; ++j) {
      const RawPoint rawPt = node(i, j);
      const RawPoint rawPt1 = poOGrid->node(i, j);
      if (rawPt1.x != rawPt.x || rawPt1.y != rawPt.y) {
        return false;
      }
    }
  }
  return true;
}

int Grid::index_of(int row, int col) const {
  if (row < 0 || col < 0 || row >= m_nRow || col >= m_nCol) {
    return -1;
  }
  return row * m_nCol + col;
}

void Grid::fill_empty_nodes() {
  nodes_.empty();
  const int n = m_nRow * m_nCol;
  for (int i = 0; i < n; ++i) {
    const OGRPoint pt(0, 0);
    nodes_.addGeometry(&pt);
  }
}

void Grid::set_size(int nRow, int nCol) {
  clear();
  m_nRow = nRow;
  m_nCol = nCol;
  if (nRow > 0 && nCol > 0) {
    fill_empty_nodes();
  }
}

void Grid::resize(int nRow, int nCol) {
  if (nRow == m_nRow && nCol == m_nCol) {
    return;
  }
  set_size(nRow, nCol);
}

void Grid::get_size(int& nRow, int& nCol) const {
  nRow = m_nRow;
  nCol = m_nCol;
}

RawPoint Grid::node(int row, int col) const {
  const int i = index_of(row, col);
  if (i < 0 || i >= nodes_.getNumGeometries()) {
    return RawPoint();
  }
  const OGRPoint* pt = nodes_.getGeometryRef(i);
  if (pt == nullptr) {
    return RawPoint();
  }
  return RawPoint(pt->getX(), pt->getY());
}

void Grid::set_node(int row, int col, const RawPoint& p) {
  const int i = index_of(row, col);
  if (i < 0 || i >= nodes_.getNumGeometries()) {
    return;
  }
  OGRPoint* pt = nodes_.getGeometryRef(i);
  if (pt == nullptr) {
    return;
  }
  pt->setX(p.x);
  pt->setY(p.y);
}

int Grid::set_nodes(const RawPoint* data, int count) {
  if (data == nullptr || count < 1 || is_empty()) {
    return SMT_ERR_FAILURE;
  }
  const int n = m_nRow * m_nCol;
  const int use = count < n ? count : n;
  for (int i = 0; i < use; ++i) {
    OGRPoint* pt = nodes_.getGeometryRef(i);
    if (pt == nullptr) {
      return SMT_ERR_FAILURE;
    }
    pt->setX(data[i].x);
    pt->setY(data[i].y);
  }
  return SMT_ERR_NONE;
}

}  // namespace geo
