// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_DOCUMENT_NAMED_VIEW_MAP_H_
#define UI_VIEWS_MARKUP_DOCUMENT_NAMED_VIEW_MAP_H_

#include "ui/ui_export.h"
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
namespace views {

class View;

// Id → View* table filled while loading markup. Does not own Views.
class UI_EXPORT NamedViewMap {
 public:
  void clear();
  void put(std::string_view id, View* view);
  View* find(std::string_view id) const;
  bool contains(std::string_view id) const;

  template <typename T>
  T* find_as(std::string_view id) const {
    return static_cast<T*>(find(id));
  }

  const std::unordered_map<std::string, View*>& entries() const {
    return map_;
  }

 private:
  std::unordered_map<std::string, View*> map_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_DOCUMENT_NAMED_VIEW_MAP_H_
