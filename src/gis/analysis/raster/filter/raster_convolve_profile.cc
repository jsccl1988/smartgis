// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Profile getters/setters are defined in raster_convolve.cc so gis.dll links
// them from the existing analysis_sources TU. This file is a GN hook: add it
// next to raster_convolve.cc; it must not define the same GIS_EXPORT symbols.

#include "gis/analysis/raster/filter/raster_convolve_profile.h"
