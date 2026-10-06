// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SCROLL_TABLE_H_
#define UI_GIS_SCROLL_TABLE_H_

#include "ui/ui_export.h"

namespace ui {
namespace views {

class ScrollView;
class TableView;

// Wrap a markup-hosted TableView in a ScrollView (Attrs / FeatureInfo pattern).
// Transfers Yoga flex-grow from the table onto the scroll viewport so the dock
// body keeps a finite height and tall row lists become scrollable.
// Returns the ScrollView (owned by the table's former parent), or nullptr.
UI_EXPORT ScrollView* wrap_markup_table_in_scroll(TableView* table,
                                                  float min_height_dip = 48.f);

// Size |table| preferred height to all rows and refresh |scroll| layout.
UI_EXPORT void sync_scroll_table_content(TableView* table,
                                         ScrollView* scroll,
                                         int width_hint = 0);

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SCROLL_TABLE_H_
