// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "sdb/feature/feature.h"

#include "ogrsf_frmts.h"

class GDALDataset;
class OGRFeature;
class OGRLayer;

class OGRGeometry;

namespace geo {
class Grid;
class Tin;
}

namespace base {
class SmtStyle;
}

namespace sdb {
namespace datasource {

sdb::SmtFeatureType infer_feature_type(OGRFeature* src,
                                           sdb::SmtFeatureType hint);

// Authoritative WKB (+ hint fields) → SmtFeatureType for an OGR layer.
// Inline so leftover callers do not need a link edge into sde_gdal.
inline sdb::SmtFeatureType feature_type_of(OGRLayer* layer) {
  if (!layer) {
    return sdb::SmtFtUnknown;
  }
  const OGRwkbGeometryType wkb = wkbFlatten(layer->GetGeomType());
  if (wkb == wkbPoint && layer->FindFieldIndex("anno", TRUE) >= 0) {
    return sdb::SmtFtAnno;
  }
  if (wkb == wkbMultiPoint && layer->FindFieldIndex("grid_row", TRUE) >= 0) {
    return sdb::SmtFtGrid;
  }
  switch (wkb) {
    case wkbPoint:
      return sdb::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return sdb::SmtFtCurve;
    case wkbPolygon:
      return layer->FindFieldIndex("area", TRUE) >= 0 ? sdb::SmtFtSurface
                                                     : sdb::SmtFtTin;
    case wkbMultiPolygon:
    case wkbTIN:
      return sdb::SmtFtTin;
    default:
      return sdb::SmtFtUnknown;
  }
}

bool encode_smt_geometry(const OGRGeometry* src, OGRFeature* dst,
                         sdb::SmtFeatureType ft);
bool encode_smt_geometry(const geo::Tin* src, OGRFeature* dst,
                         sdb::SmtFeatureType ft);
bool encode_smt_geometry(const geo::Grid* src, OGRFeature* dst,
                         sdb::SmtFeatureType ft);
OGRGeometry* decode_ogr_geometry(OGRFeature* src,
                                 sdb::SmtFeatureType hint);
geo::Tin* decode_smt_tin(OGRFeature* src);
geo::Grid* decode_smt_grid(OGRFeature* src);

void copy_smt_style_to_ogr(const base::SmtStyle* src, OGRFeature* dst);
base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src);

// Pen/brush (+ optional anno) when GeoJSON has no binary "style" blob.
// Reads optional HTML "#RRGGBB" fields fill / stroke. fblc scales anno height.
void fill_default_draw_style(OGRFeature* src, base::SmtStyle* dst, float fblc);

bool copy_ogr_feature_to_feature(OGRFeature* src, sdb::SmtFeature* dst);
inline bool copy_ogr_feature_to_smt(OGRFeature* src, sdb::SmtFeature* dst) {
  return copy_ogr_feature_to_feature(src, dst);
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         sdb::SmtFeatureType ft, OGRLayer** out);
OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
