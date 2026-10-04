// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_STYLE_DOCUMENT_H_
#define SDB_STYLE_STYLE_DOCUMENT_H_

#include <string>

#include "gis/gis_export.h"
#include "gis/style/style_types.h"

namespace gis {
namespace style {

// Parse MapLibre-subset Style JSON into StyleDocument.
GIS_EXPORT bool parse_style_document(const char* json, size_t len,
                                     StyleDocument* out);
GIS_EXPORT bool parse_style_document(const std::string& json,
                                     StyleDocument* out);

// Serialize StyleDocument back to compact JSON (v1 keys only).
GIS_EXPORT std::string serialize_style_document(const StyleDocument& doc);

}  // namespace style
}  // namespace gis

#endif  // SDB_STYLE_STYLE_DOCUMENT_H_
