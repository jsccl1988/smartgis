// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_FEATURE_H
#define _GIS_FEATURE_H

#include "attribute.h"

namespace Smt_GIS {

// Product feature kind. Storage and map documents use OGRFeature; this enum
// only labels layer geometry / extra fields (anno, tin, grid).
enum SmtFeatureType {
  SmtFtDot,
  SmtFtAnno,
  SmtFtChildImage,
  SmtFtCurve,
  SmtFtSurface,
  SmtFtGrid,
  SmtFtTin,
  SmtFtUnknown
};

}  // namespace Smt_GIS

#if !defined(Export_SmtGisCore)
#if defined(_DEBUG)
#pragma comment(lib, "SmtGisCoreD.lib")
#else
#pragma comment(lib, "SmtGisCore.lib")
#endif
#endif

#endif  // _GIS_FEATURE_H
