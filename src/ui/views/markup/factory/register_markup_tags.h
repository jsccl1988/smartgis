// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_FACTORY_REGISTER_MARKUP_TAGS_H_
#define UI_VIEWS_MARKUP_FACTORY_REGISTER_MARKUP_TAGS_H_

#include "ui/ui_export.h"

namespace ui {
namespace views {

class ControlFactory;

// Layout sugar (view/vbox/hbox/…) plus PlaceholderView and GIS/map stubs.
UI_EXPORT void register_markup_layout_tags(ControlFactory* factory);
UI_EXPORT void register_gis_placeholder_markup_tags(
    ControlFactory* factory);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_FACTORY_REGISTER_MARKUP_TAGS_H_
