// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_INGEST_OGR_INGEST_H_
#define CONTENT_BROWSER_DOCUMENT_INGEST_OGR_INGEST_H_

#include <functional>
#include <string>

#include "content/browser/document/store/layer_store.h"

namespace content {
namespace detail {

// Open a vector path via OGR into |store|. True when at least one feature
// ingested.
bool ingest_ogr_path(LayerStore* store, const std::string& path);

// Regroup a single OGR layer into area/line/point/text when kind= is present.
void split_layers_by_kind_field(LayerStore* store);

// Load accompanying *.style.json beside |path|.
void try_load_accompanying_style(
    const std::function<bool(const std::string&)>& load_style,
    const std::string& path);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_INGEST_OGR_INGEST_H_
