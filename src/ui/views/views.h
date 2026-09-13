// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_VIEWS_H_
#define UI_VIEWS_VIEWS_H_

// Public Chromium-style Views toolkit (widget / layout / events / controls).
// Product chrome composition lives in //src/app/views, not this module.
// See docs/build/ui-views-skia.md and
// docs/superpowers/specs/2026-09-13-ui-views-controls-design.md.

#include "ui/views/ambox_view.h"
#include "ui/views/att_struct_dialog.h"
#include "ui/views/attribute_table.h"
#include "ui/views/button.h"
#include "ui/views/catalog_view.h"
#include "ui/views/chart_view.h"
#include "ui/views/checkbox.h"
#include "ui/views/combobox.h"
#include "ui/views/context_menu.h"
#include "ui/views/create_datasource_dialog.h"
#include "ui/views/create_layer_dialog.h"
#include "ui/views/create_map_dialog.h"
#include "ui/views/dialog.h"
#include "ui/views/event.h"
#include "ui/views/feature_info.h"
#include "ui/views/file_picker.h"
#include "ui/views/input_text_dialog.h"
#include "ui/views/label.h"
#include "ui/views/layer_tree.h"
#include "ui/views/layout.h"
#include "ui/views/map_viewport.h"
#include "ui/views/menu_bar.h"
#include "ui/views/message_box.h"
#include "ui/views/radio_button.h"
#include "ui/views/scroll_view.h"
#include "ui/views/select_one_dialog.h"
#include "ui/views/splitter.h"
#include "ui/views/status_bar.h"
#include "ui/views/tab_strip.h"
#include "ui/views/table_view.h"
#include "ui/views/textfield.h"
#include "ui/views/theme.h"
#include "ui/views/tree_view.h"
#include "ui/views/view.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {

const char* module_id();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_VIEWS_H_
