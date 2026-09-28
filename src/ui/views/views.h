// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_VIEWS_H_
#define UI_VIEWS_VIEWS_H_

// Public Views toolkit (widget / layout / events / controls).
// Product shell composition lives in //src/app/views, not this module.
// Includes are "ui/views/<area>/<group>/foo.h". map/ stays flat.
// See docs/build/ui-views-skia.md and
// docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md.

// Kernel
#include "ui/ui_views_export.h"
#include "ui/views/kernel/shell/dialog_host.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"

// Primitives
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/button/radio_button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/input/slider.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/primitives/collection/tree_view.h"

// Dialogs
#include "ui/views/dialogs/gis/add_basemap_dialog.h"
#include "ui/views/dialogs/gis/att_struct_dialog.h"
#include "ui/views/dialogs/gis/create_datasource_dialog.h"
#include "ui/views/dialogs/gis/create_layer_dialog.h"
#include "ui/views/dialogs/gis/create_map_dialog.h"
#include "ui/views/dialogs/shell/dialog.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/input_text_dialog.h"
#include "ui/views/dialogs/shell/message_box.h"
#include "ui/views/dialogs/shell/select_one_dialog.h"

// GIS panels
#include "ui/views/gis/shell/ambox_view.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/gis/inspect/attribute_table.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/gis/panel/chart_view.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/gis/catalog/layer_tree.h"
#include "ui/views/gis/panel/processing_panel.h"
#include "ui/views/gis/shell/status_bar.h"

// Map hang
#include "ui/views/map/map_viewport.h"
#include "ui/views/map/touch_multitouch.h"

namespace ui {
namespace views {

UI_VIEWS_EXPORT const char* module_id();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_VIEWS_H_
