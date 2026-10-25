// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/ir/geometry_ir.h"

#include "gis/analysis/ops/ir/ir_visit.h"
#include "gis/geo/ops/buffer.h"
#include "gis/geo/ops/measure.h"
#include "gis/geo/ops/predicate.h"
#include "gis/geo/ops/set.h"
#include "gis/geo/ops/shape.h"
#include "gis/analysis/geometry/affine_construct.h"
#include "gis/analysis/geometry/fit.h"
#include "gis/geo/ops/transform.h"

#include "ogr_geometry.h"

#include <memory>

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

IrValue text_out(std::string text) {
  IrValue value;
  value.kind = IrKind::kText;
  value.text = std::move(text);
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
  return geometry_out(geo::intersection(*env.input, *env.other));
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
    return geometry_out(geo::centroid(*env.input));
  }
};

struct EnvelopeOp {
  static constexpr const char* id = "native.envelope";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::envelope_polygon(*env.input));
  }
};

struct UnionOp {
  static constexpr const char* id = "native.union";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geo::geom_union(*env.input, *env.other));
  }
};

struct DifferenceOp {
  static constexpr const char* id = "native.difference";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geo::difference(*env.input, *env.other));
  }
};

struct SymDifferenceOp {
  static constexpr const char* id = "native.symmetric_difference";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geo::sym_difference(*env.input, *env.other));
  }
};

struct SimplifyOp {
  static constexpr const char* id = "native.simplify";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number > 0.0 ? env.number : 0.01;
    return geometry_out(geo::simplify(*env.input, tol));
  }
};

struct ConvexHullOp {
  static constexpr const char* id = "native.convex_hull";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::convex_hull(*env.input));
  }
};

struct BoundaryOp {
  static constexpr const char* id = "native.boundary";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::boundary(*env.input));
  }
};

IrValue relate(geo::SpatialPredicate op, IrEnv& env) {
  if (!env.input || !env.other) {
    return {};
  }
  return flag_out(geo::predicate_holds(op, *env.input, *env.other));
}

struct IntersectsOp {
  static constexpr const char* id = "native.intersects";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kIntersects, env);
  }
};
struct ContainsOp {
  static constexpr const char* id = "native.contains";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kContains, env);
  }
};
struct WithinOp {
  static constexpr const char* id = "native.within";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kWithin, env);
  }
};
struct DisjointOp {
  static constexpr const char* id = "native.disjoint";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kDisjoint, env);
  }
};
struct EqualsOp {
  static constexpr const char* id = "native.equals";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kEquals, env);
  }
};
struct TouchesOp {
  static constexpr const char* id = "native.touches";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kTouches, env);
  }
};
struct CrossesOp {
  static constexpr const char* id = "native.crosses";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kCrosses, env);
  }
};
struct OverlapsOp {
  static constexpr const char* id = "native.overlaps";
  static IrValue apply(IrEnv& env) {
    return relate(geo::SpatialPredicate::kOverlaps, env);
  }
};
struct CoversOp {
  static constexpr const char* id = "native.covers";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return flag_out(geo::covers(*env.input, *env.other));
  }
};
struct CoveredByOp {
  static constexpr const char* id = "native.covered_by";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return flag_out(geo::covered_by(*env.input, *env.other));
  }
};

struct IsValidOp {
  static constexpr const char* id = "native.is_valid";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return flag_out(geo::is_valid(*env.input));
  }
};

struct IsSimpleOp {
  static constexpr const char* id = "native.is_simple";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return flag_out(geo::is_simple(*env.input));
  }
};

struct MakeValidOp {
  static constexpr const char* id = "native.make_valid";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::make_valid(*env.input));
  }
};

struct RelateOp {
  static constexpr const char* id = "native.relate";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    std::string matrix = geo::relate_de9im(*env.input, *env.other);
    if (matrix.empty()) {
      return {};
    }
    return text_out(std::move(matrix));
  }
};

struct PointOnSurfaceOp {
  static constexpr const char* id = "native.point_on_surface";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::point_on_surface(*env.input));
  }
};

struct DensifyOp {
  static constexpr const char* id = "native.densify";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number > 0.0 ? env.number : 1.0;
    return geometry_out(geo::densify(*env.input, tol));
  }
};

struct OffsetCurveOp {
  static constexpr const char* id = "native.offset_curve";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::offset_curve(*env.input, env.number));
  }
};

struct ConcaveHullOp {
  static constexpr const char* id = "native.concave_hull";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double ratio = env.number > 0.0 ? env.number : 0.0;
    return geometry_out(geo::concave_hull(*env.input, ratio));
  }
};

struct MinimumRotatedRectangleOp {
  static constexpr const char* id = "native.minimum_rotated_rectangle";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::minimum_rotated_rectangle(*env.input));
  }
};

struct MinimumBoundingCircleOp {
  static constexpr const char* id = "native.minimum_bounding_circle";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::minimum_bounding_circle(*env.input));
  }
};

struct UnaryUnionOp {
  static constexpr const char* id = "native.unary_union";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::unary_union(*env.input));
  }
};

struct LineMergeOp {
  static constexpr const char* id = "native.line_merge";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::line_merge(*env.input));
  }
};

struct NodeOp {
  static constexpr const char* id = "native.node";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::node(*env.input));
  }
};

struct PolygonizeOp {
  static constexpr const char* id = "native.polygonize";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return geometry_out(geo::polygonize(*env.input));
  }
};

struct RemoveRepeatedPointsOp {
  static constexpr const char* id = "native.remove_repeated_points";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number >= 0.0 ? env.number : 0.0;
    return geometry_out(geo::remove_repeated_points(*env.input, tol));
  }
};

struct SnapOp {
  static constexpr const char* id = "native.snap";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    const double tol = env.number > 0.0 ? env.number : 0.001;
    return geometry_out(geo::snap(*env.input, *env.other, tol));
  }
};

struct DelaunayOp {
  static constexpr const char* id = "native.delaunay";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number >= 0.0 ? env.number : 0.0;
    return geometry_out(geo::make_delaunay(*env.input, tol));
  }
};

struct VoronoiOp {
  static constexpr const char* id = "native.voronoi";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    const double tol = env.number >= 0.0 ? env.number : 0.0;
    return geometry_out(geo::voronoi(*env.input, tol));
  }
};

struct HausdorffOp {
  static constexpr const char* id = "native.hausdorff_distance";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    double dist = 0.0;
    if (!geo::hausdorff_distance(*env.input, *env.other, &dist)) {
      return {};
    }
    return number_out(dist);
  }
};

struct FrechetOp {
  static constexpr const char* id = "native.frechet_distance";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    double dist = 0.0;
    if (!geo::frechet_distance(*env.input, *env.other, &dist)) {
      return {};
    }
    return number_out(dist);
  }
};

struct ShortestLineOp {
  static constexpr const char* id = "native.shortest_line";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return geometry_out(geo::shortest_line(*env.input, *env.other));
  }
};

struct DistanceOp {
  static constexpr const char* id = "native.distance";
  static IrValue apply(IrEnv& env) {
    if (!env.input || !env.other) {
      return {};
    }
    return number_out(geo::distance(*env.input, *env.other));
  }
};

struct AreaOp {
  static constexpr const char* id = "native.area";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return number_out(geo::area(*env.input));
  }
};

struct LengthOp {
  static constexpr const char* id = "native.length";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return number_out(geo::length(*env.input));
  }
};

struct IsEmptyOp {
  static constexpr const char* id = "native.is_empty";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    return flag_out(geo::is_empty(*env.input));
  }
};

struct TransformOp {
  static constexpr const char* id = "native.transform";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    if (!geo::transform_inplace(env.input, env.src_epsg, env.dst_epsg)) {
      return {};
    }
    return flag_out(true);
  }
};

struct TranslateOp {
  static constexpr const char* id = "native.translate";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    std::unique_ptr<OGRGeometry> clone(env.input->clone());
    if (!affine_translate_2d(clone.get(), env.number, env.param_y)) {
      return {};
    }
    return geometry_out(clone.release());
  }
};

struct RotateOp {
  static constexpr const char* id = "native.rotate";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    std::unique_ptr<OGRGeometry> clone(env.input->clone());
    if (!affine_rotate_2d(clone.get(), env.number, env.origin_x, env.origin_y)) {
      return {};
    }
    return geometry_out(clone.release());
  }
};

struct ScaleOp {
  static constexpr const char* id = "native.scale";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    std::unique_ptr<OGRGeometry> clone(env.input->clone());
    if (!affine_scale_2d(clone.get(), env.number, env.param_y, env.origin_x,
                         env.origin_y)) {
      return {};
    }
    return geometry_out(clone.release());
  }
};

struct OrientedBboxOp {
  static constexpr const char* id = "native.oriented_bbox";
  static IrValue apply(IrEnv& env) {
    if (!env.input) {
      return {};
    }
    std::unique_ptr<OGRGeometry> box = oriented_bbox_from_ogr(*env.input);
    if (!box || box->IsEmpty()) {
      return {};
    }
    return geometry_out(box.release());
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
                               CoversOp,
                               CoveredByOp,
                               IsValidOp,
                               IsSimpleOp,
                               MakeValidOp,
                               RelateOp,
                               PointOnSurfaceOp,
                               DensifyOp,
                               OffsetCurveOp,
                               ConcaveHullOp,
                               MinimumRotatedRectangleOp,
                               MinimumBoundingCircleOp,
                               UnaryUnionOp,
                               LineMergeOp,
                               NodeOp,
                               PolygonizeOp,
                               RemoveRepeatedPointsOp,
                               SnapOp,
                               DelaunayOp,
                               VoronoiOp,
                               HausdorffOp,
                               FrechetOp,
                               ShortestLineOp,
                               DistanceOp,
                               AreaOp,
                               LengthOp,
                               IsEmptyOp,
                               TransformOp,
                               TranslateOp,
                               RotateOp,
                               ScaleOp,
                               OrientedBboxOp>;

}  // namespace

std::optional<IrValue> eval_geometry_ir(std::string_view id, IrEnv env) {
  return visit_apply<GeometryOps>(id, env);
}

}  // namespace detail
}  // namespace gis
