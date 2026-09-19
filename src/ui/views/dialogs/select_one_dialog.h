// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_SELECT_ONE_DIALOG_H_
#define UI_VIEWS_DIALOGS_SELECT_ONE_DIALOG_H_

#include <string>
#include <vector>

#include <windows.h>

namespace ui {
namespace views {

// Modal single-choice list that replaces leftover SmtSelectOneDlg.
// Hosts pass ids or labels; this dialog never stores GIS pointers.
class SelectOneDialog {
 public:
  static bool run(HWND owner, const std::vector<std::string>& items,
                  std::string* out);
  static bool run(HWND owner, const std::vector<unsigned int>& ids,
                  unsigned int* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_SELECT_ONE_DIALOG_H_
