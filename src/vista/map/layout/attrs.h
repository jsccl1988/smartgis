// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Feature attribute helpers and style text-field token expansion.

#ifndef GIS_VISTA_DETAIL_LAYOUT_ATTRS_H_
#define GIS_VISTA_DETAIL_LAYOUT_ATTRS_H_

#include <cstddef>
#include <string>

#include "vista/map/ir.h"

namespace vista {
namespace detail {

// Returns a reference into |batch.attrs| (or a process-static empty map).
// Avoids copying AttrMap once per style-layer × feature during layout.
const gis::style::AttrMap& attrs_at(const LayerBatch& batch, size_t index);
const char* attr_cstr(const gis::style::AttrMap& attrs, const char* key);
const char* class_cstr(const gis::style::AttrMap& attrs);
std::string expand_tokens(const std::string& field,
                          const gis::style::AttrMap& attrs);
std::string label_text(const gis::style::ResolvedPaint& paint,
                       const gis::style::AttrMap& attrs);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_DETAIL_LAYOUT_ATTRS_H_
