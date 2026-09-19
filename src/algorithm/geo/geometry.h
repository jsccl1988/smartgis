// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_GEO_GEOMETRY_H_
#define ALGORITHM_GEO_GEOMETRY_H_

#include <vector>

#include "base/core/bas_struct.h"
#include "algorithm/geo/matrix2d.h"
#include "base/carto/envelope.h"
#include "ogr_geometry.h"

#if defined(GEO_EXPORTS)
#define GEO_EXPORT_API __declspec(dllexport)
#define GEO_EXPORT_CLASS __declspec(dllexport)
#else
#define GEO_EXPORT_API __declspec(dllimport)
#define GEO_EXPORT_CLASS __declspec(dllimport)
#endif

namespace geo {

typedef base::dbfPoint RawPoint;

// Query-flag bits for leftover layer/feature filters. Not an OGR predicate API.
enum SpatialRelation {
  SS_Unkown = 0,
  SS_Within = 1,
  SS_Touches = 1 << 1,
  SS_Crosses = 1 << 2,
  SS_Overlaps = 1 << 3,
  SS_Intersects = 1 << 4,
  SS_Equals = 1 << 5,
  SS_Contains = 1 << 6,
  SS_Disjoint = 1 << 7
};

// Cutover aliases (old type names still appear in leftover call sites).
using SmtSpatialRs = SpatialRelation;

inline void copy_envelope(const OGRGeometry& geom, base::Envelope* envelope) {
  if (envelope == nullptr) {
    return;
  }
  OGREnvelope env;
  geom.getEnvelope(&env);
  envelope->MinX = env.MinX;
  envelope->MinY = env.MinY;
  envelope->MaxX = env.MaxX;
  envelope->MaxY = env.MaxY;
}

// Regular/warped XY lattice. OGR has no OGRGrid; nodes are OGRMultiPoint
// in row-major order plus explicit width/height (ortho adjacency).
class GEO_EXPORT_CLASS Grid {
 public:
  Grid();
  Grid(int nRow, int nCol);
  Grid(const Grid& other);
  Grid& operator=(const Grid& other);
  ~Grid();

  Grid* clone() const;
  void clear();
  bool is_empty() const;
  void get_envelope(base::Envelope* psEnvelope) const;

  void set_size(int nRow, int nCol);
  void resize(int nRow, int nCol);
  void get_size(int& nRow, int& nCol) const;

  RawPoint node(int row, int col) const;
  void set_node(int row, int col, const RawPoint& p);
  int set_nodes(const RawPoint* data, int count);

  const OGRMultiPoint& ogr() const { return nodes_; }
  OGRMultiPoint& ogr() { return nodes_; }

  bool equals(const Grid* other) const;

 private:
  int index_of(int row, int col) const;
  void fill_empty_nodes();

  OGRMultiPoint nodes_;
  int m_nRow;
  int m_nCol;
};

// Triangle mesh wrapping OGRTriangulatedSurface. Leftover add_point /
// get_triangle indices stay as a vertex table; render bDelete is a side
// array, not a second geometry engine.
class GEO_EXPORT_CLASS Tin {
 public:
  Tin();
  Tin(const Tin& other);
  Tin& operator=(const Tin& other);
  ~Tin();

  Tin* clone() const;
  void clear();
  bool is_empty() const;
  void get_envelope(base::Envelope* psEnvelope) const;
  void get_envelope(OGREnvelope3D* env) const;

  OGRPoint get_point(int index) const;
  int get_point_count() const { return static_cast<int>(nodes_.size()); }

  base::SmtTriangle get_triangle(int index) const;
  int get_triangle_count() const { return static_cast<int>(faces_.size()); }

  int add_point(OGRPoint* point);
  int add_point_collection(OGRPoint* points, int nPoints);
  int add_point_collection(base::dbfPoint* points, int nPoints);
  int add_point_collection(base::dbf3DPoint* points, int nPoints);
  int add_triangle(base::SmtTriangle* tri);
  int add_triangle_collection(base::SmtTriangle* tris, int nTris);
  int remove_triangle(int iIndex);

  long copy_to_tin(Tin* tin) const;

  const OGRTriangulatedSurface& ogr() const { return mesh_; }
  OGRTriangulatedSurface& ogr() { return mesh_; }

 private:
  struct Face {
    int a = -1;
    int b = -1;
    int c = -1;
  };

  int add_ogr_face(const base::SmtTriangle& tri);

  OGRTriangulatedSurface mesh_;
  std::vector<base::dbf3DPoint> nodes_;
  std::vector<Face> faces_;
  std::vector<char> deleted_;
};

typedef base::dbf3DPoint Raw3DPoint;

inline void copy_envelope3d(const OGRGeometry& geom, OGREnvelope3D* env) {
  if (env == nullptr) {
    return;
  }
  geom.getEnvelope(env);
}

// XYZ leftover terrain / model3d mesh: same OGR TIN wrapper as Tin.
class GEO_EXPORT_CLASS Surface3d : public Tin {
 public:
  Surface3d() = default;
  Surface3d(const Surface3d&) = default;
  Surface3d& operator=(const Surface3d&) = default;
  Surface3d* clone() const { return new Surface3d(*this); }
};

using Smt3DSurface = Surface3d;
using SmtTin = Tin;
using SmtGrid = Grid;

}  // namespace geo

#if !defined(GEO_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "algorithm_d.lib")
#else
#pragma comment(lib, "algorithm.lib")
#endif
#endif

#endif  // ALGORITHM_GEO_GEOMETRY_H_
