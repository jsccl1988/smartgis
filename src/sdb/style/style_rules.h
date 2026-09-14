// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_STYLE_RULES_H_
#define SDB_STYLE_STYLE_RULES_H_

#include "sdb/gis_export.h"
#include "sdb/style/style_types.h"
#include "sdb/style/symbol_library.h"

#include <vector>

namespace sdb {
namespace style {

GIS_EXPORT bool layer_matches_zoom(const StyleLayer& layer, double zoom);

GIS_EXPORT bool eval_filter(const FilterNode& filter, const AttrMap& attrs);

// Document order; optional source_layer filter (empty = any).
GIS_EXPORT std::vector<const StyleLayer*> select_layers(
    const StyleDocument& doc,
    double zoom,
    const std::string& source_layer);

// First matching layer → ResolvedPaint (with optional symbol lookup).
GIS_EXPORT bool resolve(const StyleDocument& doc,
                        const SymbolLibrary* library,
                        const AttrMap& attrs,
                        double zoom,
                        const std::string& source_layer,
                        ResolvedPaint* out);

}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_STYLE_RULES_H_
