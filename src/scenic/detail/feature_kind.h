// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_FEATURE_KIND_H_
#define SCENIC_DETAIL_FEATURE_KIND_H_

#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "gis/map/layer_kind.h"

#include "ogrsf_frmts.h"

// Scenic-local catalog feature kinds used by rhi2d carto draw. Replaces
// leftover FeatureType from legacy/gis/layer + leftover_feature.h.

namespace scenic {
namespace detail {

enum class FeatureKind {
  kDot = 0,
  kAnno,
  kChildImage,
  kCurve,
  kSurface,
  kGrid,
  kTin,
  kUnknown
};

// Historical int values (FtDot=0 …) used by Rhi2dCartoDraw::feature_type_.
enum FeatureType {
  FtDot = 0,
  FtAnno,
  FtChildImage,
  FtCurve,
  FtSurface,
  FtGrid,
  FtTin,
  FtUnknown
};

inline FeatureType feature_type_of(OGRFeature* feat) {
  if (!feat) {
    return FtUnknown;
  }
  const gis::VectorSchema schema = gis::datasource::infer_vector_schema(feat);
  switch (schema) {
    case gis::VectorSchema::kAnno:
      return FtAnno;
    case gis::VectorSchema::kChildImage:
      return FtChildImage;
    case gis::VectorSchema::kGrid:
      return FtGrid;
    case gis::VectorSchema::kTin:
      return FtTin;
    default:
      break;
  }
  switch (wkbFlatten(gis::datasource::infer_geometry_type(feat))) {
    case wkbPoint:
    case wkbMultiPoint:
      return FtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return FtCurve;
    case wkbPolygon:
    case wkbLinearRing:
    case wkbMultiPolygon:
      return FtSurface;
    case wkbTIN:
    case wkbTriangle:
      return FtTin;
    default:
      return FtUnknown;
  }
}

inline gis::VectorSchema vector_schema_of(FeatureType ft) {
  switch (ft) {
    case FtAnno:
      return gis::VectorSchema::kAnno;
    case FtChildImage:
      return gis::VectorSchema::kChildImage;
    case FtGrid:
      return gis::VectorSchema::kGrid;
    case FtTin:
      return gis::VectorSchema::kTin;
    default:
      return gis::VectorSchema::kNone;
  }
}

inline OGRGeometry* decode_geometry(OGRFeature* src, FeatureType hint) {
  return gis::datasource::decode_ogr_geometry(src, vector_schema_of(hint));
}

}  // namespace detail
}  // namespace scenic

// Historical name used by carto draw TUs (`using namespace gis` / unqualified).
namespace gis {
using FeatureType = ::scenic::detail::FeatureType;
inline constexpr FeatureType FtDot = ::scenic::detail::FtDot;
inline constexpr FeatureType FtAnno = ::scenic::detail::FtAnno;
inline constexpr FeatureType FtChildImage = ::scenic::detail::FtChildImage;
inline constexpr FeatureType FtCurve = ::scenic::detail::FtCurve;
inline constexpr FeatureType FtSurface = ::scenic::detail::FtSurface;
inline constexpr FeatureType FtGrid = ::scenic::detail::FtGrid;
inline constexpr FeatureType FtTin = ::scenic::detail::FtTin;
inline constexpr FeatureType FtUnknown = ::scenic::detail::FtUnknown;

inline FeatureType leftover_feature_type_of(OGRFeature* feat) {
  return ::scenic::detail::feature_type_of(feat);
}

inline OGRGeometry* leftover_decode_geometry(OGRFeature* src, FeatureType hint) {
  return ::scenic::detail::decode_geometry(src, hint);
}
}  // namespace gis

#endif  // SCENIC_DETAIL_FEATURE_KIND_H_
