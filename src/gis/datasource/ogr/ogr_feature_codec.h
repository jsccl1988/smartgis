// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define GIS_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "ogrsf_frmts.h"
#include "gis/datasource/ogr/ogr_feature_kind.h"
#include "gis/feature/feature.h"

class GDALDataset;
class OGRFeature;
class OGRLayer;

class OGRGeometry;

namespace gis {
namespace datasource {

gis::VectorSchema infer_vector_schema(OGRFeature* src,
                                      gis::VectorSchema hint = gis::VectorSchema::kNone);
OGRwkbGeometryType infer_geometry_type(OGRFeature* src);

bool encode_ogr_geometry(const OGRGeometry* src, OGRFeature* dst,
                         OGRwkbGeometryType wkb,
                         gis::VectorSchema schema = gis::VectorSchema::kNone);
OGRGeometry* decode_ogr_geometry(
    OGRFeature* src, gis::VectorSchema hint = gis::VectorSchema::kNone);

bool copy_ogr_feature_to_feature(OGRFeature* src, gis::Feature* dst);
inline bool copy_ogr_feature_to_smt(OGRFeature* src, gis::Feature* dst) {
  return copy_ogr_feature_to_feature(src, dst);
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         OGRwkbGeometryType wkb, gis::VectorSchema schema,
                         OGRLayer** out);
inline bool create_vector_layer(GDALDataset* ds, const char* name,
                                OGRwkbGeometryType wkb, OGRLayer** out) {
  return create_vector_layer(ds, name, wkb, gis::VectorSchema::kNone, out);
}
inline bool create_vector_layer(GDALDataset* ds, const char* name,
                                gis::VectorSchema schema, OGRLayer** out) {
  return create_vector_layer(ds, name, wkb_for_vector_schema(schema, wkbUnknown),
                             schema, out);
}

OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name);

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
