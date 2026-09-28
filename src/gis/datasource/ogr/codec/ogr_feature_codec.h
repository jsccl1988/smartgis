// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "ogrsf_frmts.h"
#include "gis/datasource/ogr/codec/ogr_feature_kind.h"
#include "gis/model/feature/feature.h"

class GDALDataset;
class OGRFeature;
class OGRLayer;

class OGRGeometry;

namespace geo {
class Grid;
class Tin;
}  // namespace geo

namespace base {
class SmtStyle;
}

namespace gis {
namespace datasource {

gis::SmtFeatureType infer_feature_type(OGRFeature* src,
                                       gis::SmtFeatureType hint);

bool encode_smt_geometry(const OGRGeometry* src, OGRFeature* dst,
                         gis::SmtFeatureType ft);
bool encode_smt_geometry(const geo::Tin* src, OGRFeature* dst,
                         gis::SmtFeatureType ft);
bool encode_smt_geometry(const geo::Grid* src, OGRFeature* dst,
                         gis::SmtFeatureType ft);
OGRGeometry* decode_ogr_geometry(OGRFeature* src, gis::SmtFeatureType hint);
geo::Tin* decode_smt_tin(OGRFeature* src);
geo::Grid* decode_smt_grid(OGRFeature* src);

void copy_smt_style_to_ogr(const base::SmtStyle* src, OGRFeature* dst);
base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src);

// Pen/brush (+ optional anno) when GeoJSON has no binary "style" blob.
// Reads optional HTML "#RRGGBB" fields fill / stroke. fblc scales anno height.
void fill_default_draw_style(OGRFeature* src, base::SmtStyle* dst, float fblc);

bool copy_ogr_feature_to_feature(OGRFeature* src, gis::SmtFeature* dst);
inline bool copy_ogr_feature_to_smt(OGRFeature* src, gis::SmtFeature* dst) {
  return copy_ogr_feature_to_feature(src, dst);
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         gis::SmtFeatureType ft, OGRLayer** out);
OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
