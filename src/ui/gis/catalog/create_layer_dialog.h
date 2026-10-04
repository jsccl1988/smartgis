// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_CATALOG_CREATE_LAYER_DIALOG_H_
#define UI_GIS_CATALOG_CREATE_LAYER_DIALOG_H_

#include "ui/ui_export.h"
#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal create-layer form: name plus Point/Line/Polygon. No Layer*.
class UI_EXPORT CreateLayerDialog {
 public:
  struct Result {
    std::string name;
    std::string geometry_type;
  };

  static bool run(HWND owner, Result* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_CATALOG_CREATE_LAYER_DIALOG_H_
