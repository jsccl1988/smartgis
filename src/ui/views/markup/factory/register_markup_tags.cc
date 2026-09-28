// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/factory/register_markup_tags.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/markup/factory/placeholder_view.h"

namespace ui {
namespace views {

void register_markup_layout_tags(ControlFactory* factory) {
  if (!factory) {
    return;
  }
  auto plain = [](std::string_view, const MarkupAttrs&) {
    return std::make_unique<View>();
  };
  factory->register_tag("view", plain);
  factory->register_tag("vbox", plain);
  factory->register_tag("hbox", plain);
  factory->register_tag("panel", plain);
  factory->register_tag("div", plain);
  factory->register_tag("contextmenu",
                        [](std::string_view, const MarkupAttrs& a) {
                          return std::make_unique<PlaceholderView>(
                              a.get("id").empty() ? "contextmenu"
                                                  : a.get("id"));
                        });
  factory->register_tag("placeholder",
                        [](std::string_view, const MarkupAttrs& a) {
                          return std::make_unique<PlaceholderView>(
                              a.get("text", "placeholder"));
                        });
}

void register_gis_placeholder_markup_tags(ControlFactory* factory) {
  if (!factory) {
    return;
  }
  const char* gis_tags[] = {
      "catalog",
      "layertree",
      "featureinfo",
      "attributetable",
      "measurepanel",
      "selectionpanel",
      "statusbar",
      "ambox",
      "atmospherepanel",
      "chartview",
      "symbologypanel",
      "legendpanel",
      "layerproperties",
      "spatialanalysis",
      "geoprocessinghistory",
      "processingpanel",
      "diagnostictools",
      "rendertrace",
      "debugconsole",
      "mapviewport",
  };
  for (const char* tag : gis_tags) {
    factory->register_tag(tag, [tag](std::string_view, const MarkupAttrs& a) {
      std::string caption = tag;
      const std::string id = a.get("id");
      if (!id.empty()) {
        caption += ":";
        caption += id;
      }
      return std::make_unique<PlaceholderView>(std::move(caption));
    });
  }
}

}  // namespace views
}  // namespace ui
