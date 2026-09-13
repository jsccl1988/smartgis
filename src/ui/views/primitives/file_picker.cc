// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/file_picker.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>

#include "ui/views/theme.h"

namespace ui {
namespace views {
namespace {

bool g_file_picker_modals_suppressed_for_test = false;

FilePickerResult pick(bool save, const wchar_t* filter) {
  FilePickerResult r;
  if (g_file_picker_modals_suppressed_for_test) {
    return r;
  }
  wchar_t path[MAX_PATH] = {};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = path;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter && filter[0] ? filter : L"All\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
  const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
  if (ok) {
    r.accepted = true;
    r.path = wide_to_utf8(path);
  }
  return r;
}

}  // namespace

void set_file_picker_modals_suppressed_for_test(bool suppressed) {
  g_file_picker_modals_suppressed_for_test = suppressed;
}

FilePickerResult pick_open_file(const wchar_t* filter) {
  return pick(false, filter);
}

FilePickerResult pick_save_file(const wchar_t* filter) {
  return pick(true, filter);
}

}  // namespace views
}  // namespace ui
