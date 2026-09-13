// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "feature.h"

class GDALDataset;
class OGRFeature;
class OGRLayer;

class OGRGeometry;

namespace Smt_Geo {
class SmtGrid;
class SmtTin;
}

namespace Smt_Base {
class SmtStyle;
}

namespace sdb {
namespace datasource {

Smt_GIS::SmtFeatureType infer_feature_type(OGRFeature* src,
                                           Smt_GIS::SmtFeatureType hint);
Smt_GIS::SmtFeatureType feature_type_of(OGRLayer* layer);

bool encode_smt_geometry(const OGRGeometry* src, OGRFeature* dst,
                         Smt_GIS::SmtFeatureType ft);
bool encode_smt_geometry(const Smt_Geo::SmtTin* src, OGRFeature* dst,
                         Smt_GIS::SmtFeatureType ft);
bool encode_smt_geometry(const Smt_Geo::SmtGrid* src, OGRFeature* dst,
                         Smt_GIS::SmtFeatureType ft);
OGRGeometry* decode_ogr_geometry(OGRFeature* src,
                                 Smt_GIS::SmtFeatureType hint);
Smt_Geo::SmtTin* decode_smt_tin(OGRFeature* src);
Smt_Geo::SmtGrid* decode_smt_grid(OGRFeature* src);

void copy_smt_style_to_ogr(const Smt_Base::SmtStyle* src, OGRFeature* dst);
Smt_Base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src);

bool create_vector_layer(GDALDataset* ds, const char* name,
                         Smt_GIS::SmtFeatureType ft, OGRLayer** out);
OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
