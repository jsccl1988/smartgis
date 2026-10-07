// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Embedded default carto Style JSON ownership (MapLibre-subset).

#ifndef GIS_STYLE_CARTO_DEFAULT_STYLE_H_
#define GIS_STYLE_CARTO_DEFAULT_STYLE_H_

#include <string>

#include "gis/gis_export.h"

namespace gis {
namespace style {

// Land / water / river / road / admin / labels / hillshade carto document.
GIS_EXPORT std::string default_carto_style_json();

}  // namespace style
}  // namespace gis

#endif  // GIS_STYLE_CARTO_DEFAULT_STYLE_H_
