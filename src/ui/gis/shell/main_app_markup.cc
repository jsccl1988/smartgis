// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/main_app_markup.h"

#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/menu/menu_bar.h"

namespace ui {
namespace views {

struct MainAppMarkupSession {
  MarkupRoot markup;
};

MainAppMarkupSession* main_app_markup_load(const char* path_or_name) {
  if (!path_or_name || !path_or_name[0]) {
    return nullptr;
  }
  auto* session = new MainAppMarkupSession();
  if (!load_markup_into(path_or_name, {}, &session->markup) ||
      !session->markup.ok()) {
    delete session;
    return nullptr;
  }
  return session;
}

static View* find_required(MainAppMarkupSession* session, const char* id) {
  if (!session) {
    return nullptr;
  }
  return session->markup.ids.find(id);
}

bool main_app_markup_fill_hosts(MainAppMarkupSession* session,
                                MainAppMarkupHosts* hosts) {
  if (!session || !hosts) {
    return false;
  }
  hosts->menu_bar =
      dynamic_cast<MenuBar*>(find_required(session, "menu_bar"));
  hosts->catalog_host = find_required(session, "catalog_host");
  hosts->map_tabs_host = find_required(session, "map_tabs_host");
  hosts->tool_bar_host = find_required(session, "tool_bar_host");
  hosts->inspector_host = find_required(session, "inspector_host");
  hosts->diagnostic_host = find_required(session, "diagnostic_host");
  hosts->status_host = find_required(session, "status_host");
  hosts->catalog_map =
      dynamic_cast<Splitter*>(find_required(session, "catalog_map"));
  hosts->main_split =
      dynamic_cast<Splitter*>(find_required(session, "main_split"));
  hosts->work = dynamic_cast<Splitter*>(find_required(session, "work"));
  hosts->map_column = find_required(session, "map_column");
  return hosts->menu_bar && hosts->catalog_host && hosts->map_tabs_host &&
         hosts->tool_bar_host && hosts->inspector_host &&
         hosts->diagnostic_host && hosts->status_host && hosts->catalog_map &&
         hosts->main_split && hosts->work;
}

bool main_app_markup_ok(MainAppMarkupSession* session) {
  return session && session->markup.ok();
}

const char* main_app_markup_error(MainAppMarkupSession* session) {
  if (!session) {
    return "main_app_markup: null session";
  }
  return session->markup.error.c_str();
}

void main_app_markup_set_root_preferred_size(MainAppMarkupSession* session,
                                             int width,
                                             int height) {
  if (!session || !session->markup.root) {
    return;
  }
  session->markup.root->set_preferred_size({width, height});
}

View* main_app_markup_take_root(MainAppMarkupSession* session) {
  if (!session) {
    return nullptr;
  }
  return session->markup.root.release();
}

void main_app_markup_release(MainAppMarkupSession* session) {
  delete session;
}

}  // namespace views
}  // namespace ui
