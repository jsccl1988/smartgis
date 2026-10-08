// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Source-layer batch index shared by fill and line painters.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_SOURCE_INDEX_H_
#define VISTA_COMPONENT_MAP_LAYOUT_SOURCE_INDEX_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "gis/style/style_types.h"
#include "vista/component/map/batch.h"

namespace vista {
namespace detail {

// Groups batch slots by source_layer so a style layer skips unrelated batches.
class SourceBatchIndex {
 public:
  explicit SourceBatchIndex(const std::vector<LayerBatch>& layers) {
    by_source_.reserve(layers.size() * 2);
    for (const LayerBatch& batch : layers) {
      by_source_[batch.source_layer].push_back(&batch);
    }
  }

  // Empty source_layer visits every batch. A named layer visits only matches.
  template <typename Fn>
  void visit(const gis::style::StyleLayer& layer,
             const std::vector<LayerBatch>& layers, Fn&& fn) const {
    if (layer.source_layer.empty()) {
      for (const LayerBatch& batch : layers) {
        fn(batch);
      }
      return;
    }
    const auto it = by_source_.find(layer.source_layer);
    if (it == by_source_.end()) {
      return;
    }
    for (const LayerBatch* batch : it->second) {
      fn(*batch);
    }
  }

 private:
  std::unordered_map<std::string, std::vector<const LayerBatch*>> by_source_;
};

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_SOURCE_INDEX_H_
