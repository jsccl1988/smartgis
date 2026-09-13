// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "sdb/feature/feature.h"

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
sdb::SmtFeatureType feature_type_of(OGRLayer* layer);

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

bool copy_ogr_feature_to_smt(OGRFeature* src, sdb::SmtFeature* dst);

bool create_vector_layer(GDALDataset* ds, const char* name,
                         sdb::SmtFeatureType ft, OGRLayer** out);
OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
