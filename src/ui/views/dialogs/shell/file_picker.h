// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_SHELL_FILE_PICKER_H_
#define UI_VIEWS_DIALOGS_SHELL_FILE_PICKER_H_

#include "ui/ui_views_export.h"
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

struct FilePickerResult {
  bool accepted = false;
  std::string path;
};

// When true, pick_* return cancelled without opening a Win32 modal.
UI_VIEWS_EXPORT void set_file_picker_modals_suppressed_for_test(bool suppressed);

UI_VIEWS_EXPORT FilePickerResult pick_open_file(const wchar_t* filter);
UI_VIEWS_EXPORT FilePickerResult pick_save_file(const wchar_t* filter);
UI_VIEWS_EXPORT FilePickerResult pick_open_file(HWND owner, const wchar_t* filter);
UI_VIEWS_EXPORT FilePickerResult pick_save_file(HWND owner, const wchar_t* filter);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_SHELL_FILE_PICKER_H_
