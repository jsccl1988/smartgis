// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_STYLE_PAINT_RESOLVE_H_
#define GIS_STYLE_PAINT_RESOLVE_H_

#include <cstdint>
#include <string>

#include "gis/gis_export.h"
#include "gis/style/style_types.h"
#include "gis/style/symbol/symbol_library.h"

namespace gis {
namespace style {

// Parse #RGB / #RRGGBB / #AARRGGBB / rgb(r,g,b) → 0xAARRGGBB.
GIS_EXPORT bool parse_color(const std::string& text, uint32_t* out_argb);

// Fill ResolvedPaint from a matched StyleLayer.
// When paint/layout values are JSON array expressions, evaluate with
// attrs+zoom.
GIS_EXPORT void fill_resolved_paint(const StyleLayer& layer,
                                    const SymbolLibrary* library,
                                    const AttrMap& attrs, double zoom,
                                    ResolvedPaint* out);

}  // namespace style
}  // namespace gis

#endif  // GIS_STYLE_PAINT_RESOLVE_H_
