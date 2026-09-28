// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_GIS_CREATE_MAP_DIALOG_H_
#define UI_VIEWS_DIALOGS_GIS_CREATE_MAP_DIALOG_H_

#include "ui/ui_views_export.h"
#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal create-map form: name only. No SmtMap*.
class UI_VIEWS_EXPORT CreateMapDialog {
 public:
  static bool run(HWND owner, std::string* name);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_GIS_CREATE_MAP_DIALOG_H_
