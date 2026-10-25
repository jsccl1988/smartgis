// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_MAIN_APP_MARKUP_H_
#define UI_GIS_SHELL_MAIN_APP_MARKUP_H_

#include "ui/ui_export.h"

namespace ui {
namespace views {

class MenuBar;
class Splitter;
class View;

// Opaque load session: MarkupRoot + NamedViewMap live only in ui_views.dll.
struct MainAppMarkupSession;

// Host views resolved from shell/main_app.ui.xml (nullptr when missing).
struct MainAppMarkupHosts {
  MenuBar* menu_bar = nullptr;
  View* catalog_host = nullptr;
  View* map_tabs_host = nullptr;
  View* tool_bar_host = nullptr;
  View* inspector_host = nullptr;
  View* diagnostic_host = nullptr;
  View* status_host = nullptr;
  Splitter* catalog_map = nullptr;
  Splitter* main_split = nullptr;
  Splitter* work = nullptr;
  View* map_column = nullptr;
};

// Load markup and fill |hosts|. Returns nullptr on failure (error via
// main_app_markup_error).
UI_EXPORT MainAppMarkupSession* main_app_markup_load(
    const char* path_or_name);

UI_EXPORT bool main_app_markup_fill_hosts(MainAppMarkupSession* session,
                                          MainAppMarkupHosts* hosts);

UI_EXPORT bool main_app_markup_ok(MainAppMarkupSession* session);

UI_EXPORT const char* main_app_markup_error(MainAppMarkupSession* session);

UI_EXPORT void main_app_markup_set_root_preferred_size(
    MainAppMarkupSession* session,
    int width,
    int height);

// Transfers root View ownership to the caller (FrameView::adopt_client).
UI_EXPORT View* main_app_markup_take_root(MainAppMarkupSession* session);

UI_EXPORT void main_app_markup_release(MainAppMarkupSession* session);

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_MAIN_APP_MARKUP_H_
