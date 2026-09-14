// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_CREATE_LAYER_DIALOG_H_
#define UI_VIEWS_CREATE_LAYER_DIALOG_H_

#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal create-layer form: name plus Point/Line/Polygon. No SmtLayer*.
class CreateLayerDialog {
 public:
  struct Result {
    std::string name;
    std::string geometry_type;
  };

  static bool run(HWND owner, Result* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_CREATE_LAYER_DIALOG_H_
