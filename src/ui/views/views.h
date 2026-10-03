// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_VIEWS_H_
#define UI_VIEWS_VIEWS_H_

// Public Views toolkit (widget / layout / events / controls).
// Product shell composition lives in //src/app/views, not this module.
// GIS panels / product dialogs live in //src/ui/gis (include "ui/gis/…").
// Includes here are "ui/views/<area>/…". dialogs/ and map/ stay flat.
// See docs/build/ui-views-skia.md and
// docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md.

// Kernel
#include "ui/ui_export.h"
#include "ui/views/kernel/paint/painter.h"
#include "ui/views/kernel/paint/painter_registry.h"
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

// Dialogs (toolkit shell only — GIS product modals live under ui/gis/catalog|inspect/)
#include "ui/views/dialogs/dialog.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/views/dialogs/message_box.h"
#include "ui/views/dialogs/select_one_dialog.h"

// Map hang
#include "ui/views/map/map_viewport.h"
#include "ui/views/map/touch_multitouch.h"

namespace ui {
namespace views {

UI_EXPORT const char* module_id();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_VIEWS_H_
