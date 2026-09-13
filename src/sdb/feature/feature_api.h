// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_API_H
#define _GIS_API_H

#include "core.h"
#include "layer.h"

class OGRLayer;

using namespace Smt_Core;
using namespace Smt_GIS;

long SMT_EXPORT_API CopyLayer(OGRLayer* pTarLayer, OGRLayer* pSrcLayer);
long SMT_EXPORT_API CopyLayer(SmtLayer* pTarLayer, SmtLayer* pSrcLayer,
                              bool bClone = true, bool bCheckFeaType = false);
long SMT_EXPORT_API CopyLayer(SmtRasterLayer* pTarLayer,
                              SmtRasterLayer* pSrcLayer, bool bClone = true,
                              bool bCheckFeaType = false);

long SMT_EXPORT_API Points2MultiPoint(OGRLayer* pLayer);

long SMT_EXPORT_API GetQueryRs(int geomType, int feaType);

#if !defined(Export_SmtGisCore)
#if defined(_DEBUG)
#pragma comment(lib, "SmtGisCoreD.lib")
#else
#pragma comment(lib, "SmtGisCore.lib")
#endif
#endif

#endif  // _GIS_API_H
