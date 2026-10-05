// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/register_markup_controls.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/detail/csv_attrs.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {

void register_primitive_markup_tags(ControlFactory* factory) {
  if (!factory) {
    return;
  }
  factory->register_tag("label", [](std::string_view, const MarkupAttrs& a) {
    return std::make_unique<Label>(a.get("text"));
  });
  factory->register_tag("textfield", [](std::string_view, const MarkupAttrs& a) {
    auto tf = std::make_unique<Textfield>();
    const std::string t = a.get("text");
    if (!t.empty()) {
      tf->set_text(t);
    }
    return tf;
  });
  factory->register_tag("button", [](std::string_view, const MarkupAttrs& a) {
    auto btn = std::make_unique<Button>(a.get("text", "Button"));
    const std::string style = a.get("style");
    if (style == "primary") {
      btn->set_style(Button::Style::kPrimary);
    } else if (style == "destructive") {
      btn->set_style(Button::Style::kDestructive);
    }
    return btn;
  });
  factory->register_tag("checkbox", [](std::string_view, const MarkupAttrs& a) {
    auto cb = std::make_unique<Checkbox>(a.get("text"));
    if (detail::attr_is_true(a.get("checked"))) {
      cb->set_checked(true);
    }
    return cb;
  });
  factory->register_tag("radiobutton",
                        [](std::string_view, const MarkupAttrs& a) {
                          const int group = std::atoi(a.get("group", "0").c_str());
                          auto rb = std::make_unique<RadioButton>(a.get("text"),
                                                                  group);
                          if (detail::attr_is_true(a.get("selected"))) {
                            rb->set_selected(true);
                          }
                          return rb;
                        });
  factory->register_tag("combobox", [](std::string_view, const MarkupAttrs& a) {
    auto combo = std::make_unique<Combobox>();
    const std::string items = a.get("items");
    if (!items.empty()) {
      for (std::string& item : detail::split_csv(items)) {
        combo->add_item(std::move(item));
      }
    }
    const std::string selected = a.get("selected");
    if (!selected.empty()) {
      combo->set_selected_index(std::atoi(selected.c_str()));
    }
    return combo;
  });
  factory->register_tag("slider", [](std::string_view, const MarkupAttrs& a) {
    auto slider = std::make_unique<Slider>();
    const std::string min_s = a.get("min");
    const std::string max_s = a.get("max");
    if (!min_s.empty() || !max_s.empty()) {
      const double min_v = min_s.empty() ? slider->min_value() : std::atof(min_s.c_str());
      const double max_v = max_s.empty() ? slider->max_value() : std::atof(max_s.c_str());
      slider->set_range(min_v, max_v);
    }
    const std::string value = a.get("value");
    if (!value.empty()) {
      slider->set_value(std::atof(value.c_str()));
    }
    return slider;
  });
  const auto make_table = [](std::string_view, const MarkupAttrs& a) {
    auto table = std::make_unique<TableView>();
    const std::string cols = a.get("columns");
    if (!cols.empty()) {
      table->set_columns(detail::split_csv(cols));
    }
    return table;
  };
  factory->register_tag("table", make_table);
  factory->register_tag("tableview", make_table);
  const auto make_tree = [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TreeView>();
  };
  factory->register_tag("tree", make_tree);
  factory->register_tag("treeview", make_tree);
  const auto make_scroll = [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<ScrollView>();
  };
  factory->register_tag("scroll", make_scroll);
  factory->register_tag("scrollview", make_scroll);
  const auto make_tabs = [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TabStrip>();
  };
  factory->register_tag("tabs", make_tabs);
  factory->register_tag("tabstrip", make_tabs);
  factory->register_tag("menubar", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<MenuBar>();
  });
}

}  // namespace views
}  // namespace ui
