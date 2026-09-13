// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_FILE_PICKER_H_
#define UI_VIEWS_FILE_PICKER_H_

#include <string>

namespace ui {
namespace views {

struct FilePickerResult {
  bool accepted = false;
  std::string path;
};

FilePickerResult pick_open_file(const wchar_t* filter);
FilePickerResult pick_save_file(const wchar_t* filter);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_FILE_PICKER_H_
