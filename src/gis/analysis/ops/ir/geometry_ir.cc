// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/ir/geometry_ir.h"

#include "gis/analysis/ops/ir/ir_visit.h"
#include "gis/analysis/ops/ir/ops_geos.h"
#include "gis/geo/ops/buffer.h"
#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/proj/coordinate_transform.h"

#include "ogr_geometry.h"

#include <string>
#include <tuple>
#include <utility>

namespace gis {
namespace detail {
namespace {

IrValue geometry_out(OGRGeometry* raw) {
  IrValue value;
  if (!raw) {
    return value;
  }
  value.kind = IrKind::kGeometry;
  value.geometry.reset(raw);
  return value;
}

IrValue flag_out(bool flag) {
  IrValue value;
  value.kind = IrKind::kFlag;
  value.flag = flag;
  return value;
}

IrValue number_out(double number) {
  IrValue value;
  value.kind = IrKind::kNumber;
  value.number = number;
  return value;
}

struct BufferOp {
  static constexpr const char* id = "native.buffer";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::buffer(*env.input, env.number));
  }
};

IrValue intersect_apply(IrEnv& env) {
  if (!env.input || !env.other) {
    return {};
  }
  return geometry_out(geos_binary(*env.input, *env.other, GeosBinary::kIntersection));
}

struct ClipOp {
  static constexpr const char* id = "native.clip";
  static IrValue apply(IrEnv& env) { return intersect_apply(env); }
};

struct IntersectionOp {
  static constexpr const char* id = "native.intersection";
  static IrValue apply(IrEnv& env) { return intersect_apply(env); }
};

struct CentroidOp {
  static constexpr const char* id = "native.centroid";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geos_centroid(*env.input));
  }
};

struct EnvelopeOp {
  static constexpr const char* id = "native.envelope";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(envelope_polygon(*env.input));
  }
};

struct UnionOp {
  static constexpr const char* id = "native.union";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geos_binary(*env.input, *env.other, GeosBinary::kUnion));
  }
};

struct DifferenceOp {
  static constexpr const char* id = "native.difference";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geos_binary(*env.input, *env.other, GeosBinary::kDifference));
  }
};

struct SymDifferenceOp {
  static constexpr const char* id = "native.symmetric_difference";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geos_binary(*env.input, *env.other, GeosBinary::kSymDifference));
  }
};

struct SimplifyOp {
  static constexpr const char* id = "native.simplify";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number > 0.0 ? env.number : 0.01;
    return geometry_out(geos_simplify(*env.input, tol));
  }
};

struct ConvexHullOp {
  static constexpr const char* id = "native.convex_hull";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geos_convex_hull(*env.input));
  }
};

struct BoundaryOp {
  static constexpr const char* id = "native.boundary";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geos_boundary(*env.input));
  }
};

enum class Predicate {
  kIntersects,
  kContains,
  kWithin,
  kDisjoint,
  kEquals,
  kTouches,
  kCrosses,
  kOverlaps,
};

// True when the two geometries intersect.
struct IntersectsPred {
  static constexpr Predicate id = Predicate::kIntersects;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Intersects(&right) != 0;
  }
};

// True when left contains right.
struct ContainsPred {
  static constexpr Predicate id = Predicate::kContains;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Contains(&right) != 0;
  }
};

// True when left is within right.
struct WithinPred {
  static constexpr Predicate id = Predicate::kWithin;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Within(&right) != 0;
  }
};

// True when the geometries share no points.
struct DisjointPred {
  static constexpr Predicate id = Predicate::kDisjoint;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Disjoint(&right) != 0;
  }
};

// True when the geometries are spatially equal.
struct EqualsPred {
  static constexpr Predicate id = Predicate::kEquals;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Equals(&right) != 0;
  }
};

// True when the geometries touch at a boundary only.
struct TouchesPred {
  static constexpr Predicate id = Predicate::kTouches;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Touches(&right) != 0;
  }
};

// True when the geometries cross.
struct CrossesPred {
  static constexpr Predicate id = Predicate::kCrosses;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Crosses(&right) != 0;
  }
};

// True when the geometries overlap.
struct OverlapsPred {
  static constexpr Predicate id = Predicate::kOverlaps;
  static bool apply(const OGRGeometry& left, const OGRGeometry& right) {
    return left.Overlaps(&right) != 0;
  }
};

using PredicateFns = std::tuple<IntersectsPred,
                                ContainsPred,
                                WithinPred,
                                DisjointPred,
                                EqualsPred,
                                TouchesPred,
                                CrossesPred,
                                OverlapsPred>;

bool predicate_holds(Predicate op, const OGRGeometry& left, const OGRGeometry& right) {
  if (left.IsEmpty() || right.IsEmpty()) {
    return op == Predicate::kDisjoint;
  }
  const auto hit = visit_apply<PredicateFns>(op, left, right);
  return hit ? *hit : false;
}

IrValue relate(Predicate op, IrEnv& env) {
  if (!env.input || !env.other) {
    return {};
  }
  return flag_out(predicate_holds(op, *env.input, *env.other));
}

struct IntersectsOp {
  static constexpr const char* id = "native.intersects";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kIntersects, env); }
};
struct ContainsOp {
  static constexpr const char* id = "native.contains";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kContains, env); }
};
struct WithinOp {
  static constexpr const char* id = "native.within";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kWithin, env); }
};
struct DisjointOp {
  static constexpr const char* id = "native.disjoint";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kDisjoint, env); }
};
struct EqualsOp {
  static constexpr const char* id = "native.equals";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kEquals, env); }
};
struct TouchesOp {
  static constexpr const char* id = "native.touches";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kTouches, env); }
};
struct CrossesOp {
  static constexpr const char* id = "native.crosses";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kCrosses, env); }
};
struct OverlapsOp {
  static constexpr const char* id = "native.overlaps";
  static IrValue apply(IrEnv& env) { return relate(Predicate::kOverlaps, env); }
};

struct DistanceOp {
  static constexpr const char* id = "native.distance";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return number_out(env.input->Distance(env.other));
  }
};

struct AreaOp {
  static constexpr const char* id = "native.area";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    double measured = 0;
    if (OGRSurface* surface = env.input->toSurface()) {
      measured = surface->get_Area();
    } else if (OGRGeometryCollection* collection = env.input->toGeometryCollection()) {
      for (int i = 0; i < collection->getNumGeometries(); ++i) {
        if (OGRSurface* part = collection->getGeometryRef(i)->toSurface()) {
          measured += part->get_Area();
        }
      }
    }
    return number_out(measured);
  }
};

struct LengthOp {
  static constexpr const char* id = "native.length";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    double measured = 0;
    const OGRwkbGeometryType flat = wkbFlatten(env.input->getGeometryType());
    if (flat == wkbLineString || flat == wkbLinearRing) {
      measured = static_cast<OGRLineString*>(env.input)->get_Length();
    } else if (OGRGeometryCollection* collection = env.input->toGeometryCollection()) {
      for (int i = 0; i < collection->getNumGeometries(); ++i) {
        OGRGeometry* part = collection->getGeometryRef(i);
        const OGRwkbGeometryType part_flat = wkbFlatten(part->getGeometryType());
        if (part_flat == wkbLineString || part_flat == wkbLinearRing) {
          measured += static_cast<OGRLineString*>(part)->get_Length();
        }
      }
    }
    return number_out(measured);
  }
};

struct IsEmptyOp {
  static constexpr const char* id = "native.is_empty";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return flag_out(env.input->IsEmpty());
  }
};

bool map_curve(OGRLineString* line, const geo::CoordinateTransform& transform, int dim) {
  if (!line) {
    return true;
  }
  for (int i = 0; i < line->getNumPoints(); ++i) {
    double x = line->getX(i);
    double y = line->getY(i);
    const double z = dim >= 3 ? line->getZ(i) : 0.0;
    if (!transform.transform_xy(x, y)) {
      return false;
    }
    if (dim >= 3) {
      line->setPoint(i, x, y, z);
    } else {
      line->setPoint(i, x, y);
    }
  }
  return true;
}

bool transform_geometry(OGRGeometry* geom, const geo::CoordinateTransform& transform) {
  if (!geom || geom->IsEmpty()) {
    return geom != nullptr;
  }
  const int dim = geo::geometry_traits<OGRGeometry>::coordinate_dimension(*geom);
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    auto* point = static_cast<OGRPoint*>(geom);
    double x = point->getX();
    double y = point->getY();
    if (!transform.transform_xy(x, y)) {
      return false;
    }
    point->setX(x);
    point->setY(y);
    return true;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    return map_curve(static_cast<OGRLineString*>(geom), transform, dim);
  }
  if (flat == wkbPolygon) {
    auto* polygon = static_cast<OGRPolygon*>(geom);
    if (!map_curve(polygon->getExteriorRing(), transform, dim)) {
      return false;
    }
    for (int i = 0; i < polygon->getNumInteriorRings(); ++i) {
      if (!map_curve(polygon->getInteriorRing(i), transform, dim)) {
        return false;
      }
    }
    return true;
  }
  if (OGRGeometryCollection* collection = geom->toGeometryCollection()) {
    for (int i = 0; i < collection->getNumGeometries(); ++i) {
      if (!transform_geometry(collection->getGeometryRef(i), transform)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

struct TransformOp {
  static constexpr const char* id = "native.transform";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const std::string src_crs = "EPSG:" + std::to_string(env.src_epsg);
    const std::string dst_crs = "EPSG:" + std::to_string(env.dst_epsg);
    const geo::CoordinateTransform xform(src_crs, dst_crs);
    if (!xform.is_valid() || !transform_geometry(env.input, xform)) {
      return {};
    }
    return flag_out(true);
  }
};

using GeometryOps = std::tuple<BufferOp,
                               ClipOp,
                               CentroidOp,
                               EnvelopeOp,
                               IntersectionOp,
                               UnionOp,
                               DifferenceOp,
                               SymDifferenceOp,
                               SimplifyOp,
                               ConvexHullOp,
                               BoundaryOp,
                               IntersectsOp,
                               ContainsOp,
                               WithinOp,
                               DisjointOp,
                               EqualsOp,
                               TouchesOp,
                               CrossesOp,
                               OverlapsOp,
                               DistanceOp,
                               AreaOp,
                               LengthOp,
                               IsEmptyOp,
                               TransformOp>;

}  // namespace

std::optional<IrValue> eval_geometry_ir(std::string_view id, IrEnv env) {
  return visit_apply<GeometryOps>(id, env);
}

}  // namespace detail
}  // namespace gis
