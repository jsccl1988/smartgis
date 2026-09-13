// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

#include "feature.h"

class OGRFeature;

namespace sdb {
namespace datasource {

bool copy_ogr_feature_to_smt(OGRFeature* src, Smt_GIS::SmtFeature* dst);
bool copy_ogr_feature_to_smt(OGRFeature* src, Smt_GIS::SmtFeature* dst,
                             Smt_GIS::SmtFeatureType hint);
bool copy_smt_feature_to_ogr(const Smt_GIS::SmtFeature* src, OGRFeature* dst);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
