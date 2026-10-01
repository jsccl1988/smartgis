// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/register_markup_controls.h"

#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace {

std::vector<std::string> split_csv(std::string_view raw) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : raw) {
    if (c == ',') {
      while (!cur.empty() &&
             std::isspace(static_cast<unsigned char>(cur.front()))) {
        cur.erase(cur.begin());
      }
      while (!cur.empty() &&
             std::isspace(static_cast<unsigned char>(cur.back()))) {
        cur.pop_back();
      }
      if (!cur.empty()) {
        out.push_back(cur);
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  while (!cur.empty() &&
         std::isspace(static_cast<unsigned char>(cur.front()))) {
    cur.erase(cur.begin());
  }
  while (!cur.empty() &&
         std::isspace(static_cast<unsigned char>(cur.back()))) {
    cur.pop_back();
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

}  // namespace

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
    return std::make_unique<Button>(a.get("text", "Button"));
  });
  factory->register_tag("checkbox", [](std::string_view, const MarkupAttrs& a) {
    auto cb = std::make_unique<Checkbox>(a.get("text"));
    if (a.get("checked") == "true" || a.get("checked") == "1") {
      cb->set_checked(true);
    }
    return cb;
  });
  factory->register_tag("radiobutton",
                        [](std::string_view, const MarkupAttrs& a) {
                          const int group = std::atoi(a.get("group", "0").c_str());
                          auto rb = std::make_unique<RadioButton>(a.get("text"),
                                                                  group);
                          if (a.get("selected") == "true" ||
                              a.get("selected") == "1") {
                            rb->set_selected(true);
                          }
                          return rb;
                        });
  factory->register_tag("combobox", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<Combobox>();
  });
  factory->register_tag("slider", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<Slider>();
  });
  factory->register_tag("table", [](std::string_view, const MarkupAttrs& a) {
    auto table = std::make_unique<TableView>();
    const std::string cols = a.get("columns");
    if (!cols.empty()) {
      table->set_columns(split_csv(cols));
    }
    return table;
  });
  factory->register_tag("tableview", [](std::string_view, const MarkupAttrs& a) {
    auto table = std::make_unique<TableView>();
    const std::string cols = a.get("columns");
    if (!cols.empty()) {
      table->set_columns(split_csv(cols));
    }
    return table;
  });
  factory->register_tag("tree", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TreeView>();
  });
  factory->register_tag("treeview", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TreeView>();
  });
  factory->register_tag("scroll", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<ScrollView>();
  });
  factory->register_tag("scrollview", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<ScrollView>();
  });
  factory->register_tag("tabs", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TabStrip>();
  });
  factory->register_tag("tabstrip", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<TabStrip>();
  });
  factory->register_tag("menubar", [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<MenuBar>();
  });
}

}  // namespace views
}  // namespace ui
