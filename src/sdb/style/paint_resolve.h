// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_PAINT_RESOLVE_H_
#define SDB_STYLE_PAINT_RESOLVE_H_

#include "sdb/carto/style.h"
#include "sdb/gis_export.h"
#include "sdb/style/style_types.h"
#include "sdb/style/symbol_library.h"

#include <cstdint>
#include <string>

namespace sdb {
namespace style {

// Parse #RGB / #RRGGBB / #AARRGGBB / rgb(r,g,b) → 0xAARRGGBB.
GIS_EXPORT bool parse_color(const std::string& text, uint32_t* out_argb);

// Fill ResolvedPaint fields from a matched StyleLayer (+ optional library).
GIS_EXPORT void fill_resolved_paint(const StyleLayer& layer,
                                    const SymbolLibrary* library,
                                    ResolvedPaint* out);

// Bridge to legacy cartographic POD for Feature / leftover render.
GIS_EXPORT base::SmtStyle to_smt_style(const ResolvedPaint& paint,
                                       const char* name);

}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_PAINT_RESOLVE_H_
