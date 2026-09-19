// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_CREATE_DATASOURCE_DIALOG_H_
#define UI_VIEWS_DIALOGS_CREATE_DATASOURCE_DIALOG_H_

#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal create-datasource form: name plus MEM / file / GPKG labels only.
class CreateDatasourceDialog {
 public:
  struct Result {
    std::string name;
    std::string type;
  };

  static bool run(HWND owner, Result* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_CREATE_DATASOURCE_DIALOG_H_
