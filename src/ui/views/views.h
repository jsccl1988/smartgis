// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_VIEWS_H_
#define UI_VIEWS_VIEWS_H_

// Public Chromium-style Views toolkit (widget / layout / events / controls).
// Product chrome composition lives in //src/app/views, not this module.
// Includes are responsibility-partitioned under kernel|primitives|dialogs|gis|map.
// See docs/build/ui-views-skia.md and
// docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md.

// Kernel
#include "ui/views/kernel/dialog_host.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/kernel/event.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/kernel/layout_check.h"
#include "ui/views/kernel/splitter.h"
#include "ui/views/kernel/theme.h"
#include "ui/views/kernel/view.h"
#include "ui/views/kernel/widget.h"

// Primitives
#include "ui/views/primitives/button.h"
#include "ui/views/primitives/checkbox.h"
#include "ui/views/primitives/combobox.h"
#include "ui/views/primitives/context_menu.h"
#include "ui/views/primitives/label.h"
#include "ui/views/primitives/menu_bar.h"
#include "ui/views/primitives/radio_button.h"
#include "ui/views/primitives/scroll_view.h"
#include "ui/views/primitives/tab_strip.h"
#include "ui/views/primitives/table_view.h"
#include "ui/views/primitives/textfield.h"
#include "ui/views/primitives/tree_view.h"

// Dialogs
#include "ui/views/dialogs/add_basemap_dialog.h"
#include "ui/views/dialogs/att_struct_dialog.h"
#include "ui/views/dialogs/create_datasource_dialog.h"
#include "ui/views/dialogs/create_layer_dialog.h"
#include "ui/views/dialogs/create_map_dialog.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/dialogs/select_one_dialog.h"

// GIS panels
#include "ui/views/gis/ambox_view.h"
#include "ui/views/gis/attribute_table.h"
#include "ui/views/gis/catalog_view.h"
#include "ui/views/gis/chart_view.h"
#include "ui/views/gis/feature_info.h"
#include "ui/views/gis/layer_tree.h"
#include "ui/views/gis/status_bar.h"

// Map hang
#include "ui/views/map/map_viewport.h"
#include "ui/views/map/touch_multitouch.h"

namespace ui {
namespace views {

const char* module_id();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_VIEWS_H_
