// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_INPUT_TEXT_DIALOG_H_
#define UI_VIEWS_INPUT_TEXT_DIALOG_H_

#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal prompt that replaces leftover SmtInputTextDlg. Strings only; no GIS
// types. Writes the accepted text to |out|.
class InputTextDialog {
 public:
  static bool run(HWND owner, std::string* out);
  static bool run(HWND owner, const wchar_t* title, const std::string& prompt,
                  std::string* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_INPUT_TEXT_DIALOG_H_
