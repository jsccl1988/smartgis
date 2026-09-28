// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/document/named_view_map.h"

namespace ui {
namespace views {

void NamedViewMap::clear() {
  map_.clear();
}

void NamedViewMap::put(std::string_view id, View* view) {
  if (id.empty() || !view) {
    return;
  }
  map_[std::string(id)] = view;
}

View* NamedViewMap::find(std::string_view id) const {
  const auto it = map_.find(std::string(id));
  if (it == map_.end()) {
    return nullptr;
  }
  return it->second;
}

bool NamedViewMap::contains(std::string_view id) const {
  return map_.find(std::string(id)) != map_.end();
}

}  // namespace views
}  // namespace ui
