// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_DIALOGS_CREATE_MAP_DIALOG_H_
#define UI_GIS_DIALOGS_CREATE_MAP_DIALOG_H_

#include "ui/ui_export.h"
#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal create-map form: name only. No SmtMap*.
class UI_EXPORT CreateMapDialog {
 public:
  static bool run(HWND owner, std::string* name);
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_DIALOGS_CREATE_MAP_DIALOG_H_
