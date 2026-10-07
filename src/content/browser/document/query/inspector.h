// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_QUERY_INSPECTOR_H_
#define CONTENT_BROWSER_DOCUMENT_QUERY_INSPECTOR_H_

#include <string>
#include <utility>
#include <vector>

#include "content/browser/document/store/layer_store.h"
#include "content/browser/document/style/style_bind.h"

namespace content {
namespace detail {

void fill_feature_info_fields(
    const LayerStore& store, const StyleBind& style, const GisFeature& f,
    std::vector<std::pair<std::string, std::string>>* out,
    const std::string& source_layer, double map_scale);

void fill_attribute_rows(const LayerStore& store,
                         std::vector<std::string>* columns,
                         std::vector<std::vector<std::string>>* rows,
                         std::vector<std::string>* tokens);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_QUERY_INSPECTOR_H_
