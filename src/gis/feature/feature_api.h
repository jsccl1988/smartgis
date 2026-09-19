// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_API_H
#define _GIS_API_H

#include "base/core/core.h"
#include "gis/gis_export.h"
#include "gis/layer/layer.h"

class OGRFeature;
class OGRLayer;

using namespace base;
using namespace gis;

// Copy geometry (and matching fields) onto dest's feature defn.
// CreateFeature(src) fails when src was built against another layer.
long GIS_EXPORT append_cloned_feature(OGRLayer* dest, const OGRFeature* src);
long GIS_EXPORT copy_layer(OGRLayer* pTarLayer, OGRLayer* pSrcLayer);
long GIS_EXPORT copy_layer(SmtLayer* pTarLayer, SmtLayer* pSrcLayer,
                           bool bClone = true, bool bCheckFeaType = false);
long GIS_EXPORT copy_layer(SmtRasterLayer* pTarLayer, SmtRasterLayer* pSrcLayer,
                           bool bClone = true, bool bCheckFeaType = false);

long GIS_EXPORT points_to_multi_point(OGRLayer* pLayer);

long GIS_EXPORT get_query_rs(int geomType, int feaType);

#endif  // _GIS_API_H
