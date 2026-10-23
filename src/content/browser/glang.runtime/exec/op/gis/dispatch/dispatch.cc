// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GIS opcode table. Eval functions live under op/gis/eval. Traits bind a
// GisCapability method; install_ops is the only runner.

#include "content/browser/glang.runtime/exec/op/register.h"

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/gis/eval/eval.h"
#include "content/browser/glang.runtime/exec/op/gis/eval/host_call.h"
#include "content/browser/glang.runtime/exec/op/install.h"

#include <tuple>
#include <utility>

namespace content {
namespace code {
namespace {

constexpr Param kBinaryParams[] = {
    {"a", 0, ArgKind::kStr, true, 0, nullptr},
    {"b", 1, ArgKind::kStr, true, 0, nullptr},
};
constexpr Param kUnaryParams[] = {
    {"a", 0, ArgKind::kStr, true, 0, nullptr},
};
constexpr Param kTransformParams[] = {
    {"a", 0, ArgKind::kStr, true, 0, nullptr},
    {"dst_epsg", 1, ArgKind::kI32, true, 0, nullptr},
};
constexpr Param kPathParams[] = {
    {"path", 0, ArgKind::kPath, true, 0, nullptr},
};
constexpr Param kFilterParams[] = {
    {"path", 0, ArgKind::kPath, true, 0, nullptr},
    {"where", 1, ArgKind::kStr, false, 0, ""},
    {"wkt", 2, ArgKind::kStr, false, 0, ""},
};
constexpr Param kFieldParams[] = {
    {"path", 0, ArgKind::kPath, true, 0, nullptr},
    {"fid", 1, ArgKind::kI32, true, 0, nullptr},
    {"field", 2, ArgKind::kStr, true, 0, nullptr},
};
constexpr Param kProjectParams[] = {
    {"path", 0, ArgKind::kPath, true, 0, nullptr},
    {"fields", 1, ArgKind::kStr, true, 0, nullptr},
};
constexpr Param kModeParams[] = {
    {"mode", 0, ArgKind::kStr, true, 0, nullptr},
};

// Binary GisCapability method bound as an opcode.
template <Op kId, auto kName, auto kMethod>
struct HostBinaryOp {
  static constexpr Op id = kId;
  static constexpr auto stored_name = kName;
  static constexpr const char* name = stored_name.text;
  static constexpr const Param* params = kBinaryParams;
  static constexpr int arity = 2;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_host_binary<kMethod>(host, module, inst, slots, error);
  }
};

// Unary GisCapability method bound as an opcode.
template <Op kId, auto kName, auto kMethod>
struct HostUnaryOp {
  static constexpr Op id = kId;
  static constexpr auto stored_name = kName;
  static constexpr const char* name = stored_name.text;
  static constexpr const Param* params = kUnaryParams;
  static constexpr int arity = 1;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_host_unary<kMethod>(host, module, inst, slots, error);
  }
};

// Buffer a geometry by a distance in the same projected CRS.
struct BufferOp {
  static constexpr Op id = Op::kBuffer;
  static constexpr const char* name = "buffer";
  static constexpr const Param* params = kBinaryParams;
  static constexpr int arity = 2;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_buffer(host, module, inst, slots, error);
  }
};

// Simplify a line or polygon by a tolerance.
struct SimplifyOp {
  static constexpr Op id = Op::kSimplify;
  static constexpr const char* name = "simplify";
  static constexpr const Param* params = kBinaryParams;
  static constexpr int arity = 2;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_simplify(host, module, inst, slots, error);
  }
};

// Reproject a geometry from its source EPSG to a destination EPSG.
struct TransformOp {
  static constexpr Op id = Op::kTransform;
  static constexpr const char* name = "transform";
  static constexpr const Param* params = kTransformParams;
  static constexpr int arity = 2;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_transform(host, module, inst, slots, error);
  }
};

// List features of a vector dataset as JSON.
struct ScanOp {
  static constexpr Op id = Op::kScanFeatures;
  static constexpr const char* name = "scan_features";
  static constexpr const Param* params = kPathParams;
  static constexpr int arity = 1;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_scan(host, module, inst, slots, error);
  }
};

// Filter features by an attribute clause and an optional WKT.
struct FilterOp {
  static constexpr Op id = Op::kFilterFeatures;
  static constexpr const char* name = "filter_features";
  static constexpr const Param* params = kFilterParams;
  static constexpr int arity = 3;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_filter(host, module, inst, slots, error);
  }
};

// Read one attribute of one feature.
struct ReadFieldOp {
  static constexpr Op id = Op::kReadFeatureField;
  static constexpr const char* name = "read_feature_field";
  static constexpr const Param* params = kFieldParams;
  static constexpr int arity = 3;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_read_field(host, module, inst, slots, error);
  }
};

// Project a feature list down to named fields.
struct ProjectOp {
  static constexpr Op id = Op::kProjectFeatures;
  static constexpr const char* name = "project_features";
  static constexpr const Param* params = kProjectParams;
  static constexpr int arity = 2;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_project(host, module, inst, slots, error);
  }
};

// JSON list of open document layers.
struct ListLayersOp {
  static constexpr Op id = Op::kListLayers;
  static constexpr const char* name = "list_layers";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_list_layers(host, module, inst, slots, error);
  }
};

// Feature count of the bound document. Python callers read the count string.
struct FeatureCountOp {
  static constexpr Op id = Op::kFeatureCount;
  static constexpr const char* name = "feature_count";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_feature_count(host, module, inst, slots, error);
  }
};

// JSON extent of the bound map.
struct MapExtentOp {
  static constexpr Op id = Op::kMapExtent;
  static constexpr const char* name = "map_extent";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_map_extent(host, module, inst, slots, error);
  }
};

// Current view present mode.
struct PresentModeOp {
  static constexpr Op id = Op::kPresentMode;
  static constexpr const char* name = "present_mode";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_present_mode(host, module, inst, slots, error);
  }
};

// Set the view present mode.
struct SetPresentModeOp {
  static constexpr Op id = Op::kSetPresentMode;
  static constexpr const char* name = "set_present_mode";
  static constexpr const Param* params = kModeParams;
  static constexpr int arity = 1;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_set_present_mode(host, module, inst, slots, error);
  }
};

// Whether the document has a style.
struct StyleHasOp {
  static constexpr Op id = Op::kStyleHasDocument;
  static constexpr const char* name = "style_has_document";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_style_has(host, module, inst, slots, error);
  }
};

// Clear the document style.
struct StyleClearOp {
  static constexpr Op id = Op::kStyleClear;
  static constexpr const char* name = "style_clear";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_style_clear(host, module, inst, slots, error);
  }
};

// JSON summary of the document style.
struct StyleSummaryOp {
  static constexpr Op id = Op::kStyleSummary;
  static constexpr const char* name = "style_summary";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_style_summary(host, module, inst, slots, error);
  }
};

// JSON catalog of builtin analysis operators.
struct ListAnalysisOp {
  static constexpr Op id = Op::kListAnalysisOps;
  static constexpr const char* name = "list_analysis_ops";
  static constexpr const Param* params = nullptr;
  static constexpr int arity = 0;
  static constexpr bool produces = true;
  static bool apply(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
    return detail::eval_list_analysis(host, module, inst, slots, error);
  }
};

using IntersectionOp = HostBinaryOp<Op::kIntersection,
                                    detail::OpName("intersection"),
                                    &content::GisCapability::intersection>;
using UnionOp = HostBinaryOp<Op::kUnion, detail::OpName("geom_union"), &content::GisCapability::geom_union>;
using DifferenceOp =
    HostBinaryOp<Op::kDifference, detail::OpName("difference"), &content::GisCapability::difference>;
using SymDifferenceOp = HostBinaryOp<Op::kSymDifference,
                                     detail::OpName("symmetric_difference"),
                                     &content::GisCapability::sym_difference>;
using CentroidOp = HostUnaryOp<Op::kCentroid, detail::OpName("centroid"), &content::GisCapability::centroid>;
using EnvelopeOp = HostUnaryOp<Op::kEnvelope, detail::OpName("envelope"), &content::GisCapability::envelope>;
using ConvexHullOp =
    HostUnaryOp<Op::kConvexHull, detail::OpName("convex_hull"), &content::GisCapability::convex_hull>;
using BoundaryOp = HostUnaryOp<Op::kBoundary, detail::OpName("boundary"), &content::GisCapability::boundary>;
using IntersectsOp =
    HostBinaryOp<Op::kIntersects, detail::OpName("intersects"), &content::GisCapability::intersects>;
using ContainsOp = HostBinaryOp<Op::kContains, detail::OpName("contains"), &content::GisCapability::contains>;
using WithinOp = HostBinaryOp<Op::kWithin, detail::OpName("within"), &content::GisCapability::within>;
using DisjointOp = HostBinaryOp<Op::kDisjoint, detail::OpName("disjoint"), &content::GisCapability::disjoint>;
using EqualsOp = HostBinaryOp<Op::kEquals, detail::OpName("geom_equals"), &content::GisCapability::equals>;
using TouchesOp = HostBinaryOp<Op::kTouches, detail::OpName("touches"), &content::GisCapability::touches>;
using CrossesOp = HostBinaryOp<Op::kCrosses, detail::OpName("crosses"), &content::GisCapability::crosses>;
using OverlapsOp = HostBinaryOp<Op::kOverlaps, detail::OpName("overlaps"), &content::GisCapability::overlaps>;
using GeomDistanceOp =
    HostBinaryOp<Op::kGeomDistance, detail::OpName("geom_distance"), &content::GisCapability::distance>;
using GeomAreaOp = HostUnaryOp<Op::kGeomArea, detail::OpName("geom_area"), &content::GisCapability::area>;
using GeomLengthOp =
    HostUnaryOp<Op::kGeomLength, detail::OpName("geom_length"), &content::GisCapability::length>;
using IsEmptyOp = HostUnaryOp<Op::kIsEmpty, detail::OpName("is_empty"), &content::GisCapability::is_empty>;

using GisOpTable = std::tuple<IntersectionOp,
                              UnionOp,
                              DifferenceOp,
                              SymDifferenceOp,
                              CentroidOp,
                              EnvelopeOp,
                              ConvexHullOp,
                              BoundaryOp,
                              SimplifyOp,
                              TransformOp,
                              IntersectsOp,
                              ContainsOp,
                              WithinOp,
                              DisjointOp,
                              EqualsOp,
                              TouchesOp,
                              CrossesOp,
                              OverlapsOp,
                              GeomDistanceOp,
                              GeomAreaOp,
                              GeomLengthOp,
                              IsEmptyOp,
                              ScanOp,
                              FilterOp,
                              ReadFieldOp,
                              ProjectOp,
                              ListLayersOp,
                              FeatureCountOp,
                              MapExtentOp,
                              PresentModeOp,
                              SetPresentModeOp,
                              StyleHasOp,
                              StyleClearOp,
                              StyleSummaryOp,
                              ListAnalysisOp>;
}  // namespace

void fill_buffer_op(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  detail::install_ops<std::tuple<BufferOp>>(by_op);
}

void fill_gis_ops(const OpDesc** by_op) {
  detail::install_ops<GisOpTable>(by_op);
}

}  // namespace code
}  // namespace content
