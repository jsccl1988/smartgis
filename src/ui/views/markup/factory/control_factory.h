// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_FACTORY_CONTROL_FACTORY_H_
#define UI_VIEWS_MARKUP_FACTORY_CONTROL_FACTORY_H_

#include "ui/ui_export.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ui {
namespace views {

class View;

// Attribute bag for XML element construction (keeps pugixml out of the
// public factory header).
struct UI_EXPORT MarkupAttrs {
  std::string get(std::string_view key, std::string_view fallback = {}) const;
  std::unordered_map<std::string, std::string> values;
};

// Maps XML tag names to View constructors. Registry-only (no MarkupDocument /
// Yoga). Concrete control registration lives in primitives/markup aggregation
// TUs (see make_default). GN: :views_control_factory — not :views_markup.
class UI_EXPORT ControlFactory {
 public:
  using Creator =
      std::function<std::unique_ptr<View>(std::string_view tag,
                                          const MarkupAttrs& attrs)>;

  ControlFactory();

  void register_tag(std::string_view tag, Creator creator);
  bool has_tag(std::string_view tag) const;

  // Returns nullptr for unknown tags.
  std::unique_ptr<View> create(std::string_view tag,
                               const MarkupAttrs& attrs) const;

  std::vector<std::string> registered_tags() const;

  // Layout sugar + README primitives + GIS placeholders (aggregation TU).
  static ControlFactory make_default();

 private:
  std::unordered_map<std::string, Creator> creators_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_FACTORY_CONTROL_FACTORY_H_
