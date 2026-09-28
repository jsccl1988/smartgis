// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_INGEST_GEOJSON_WRITE_H_
#define CONTENT_BROWSER_DOCUMENT_INGEST_GEOJSON_WRITE_H_

#include <string>

#include "content/browser/document/store/layer_store.h"

namespace content {
namespace detail {

// Write the active visible layer (or first visible non-empty) as GeoJSON.
bool write_geojson_path(const LayerStore& store, const std::string& path);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_INGEST_GEOJSON_WRITE_H_
