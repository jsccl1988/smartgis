// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_SHOWCASE_HOST_H_
#define LEGACY_APP_SHELL_SHOWCASE_HOST_H_

#include <cstdio>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace legacy_app {

// Shared blank popup HWND + mark helpers for legacy map2d/scene3d showcases.
// STATIC DefWindowProc paints white; empty paint retains direct blit / present.

inline void write_showcase_mark(const char *mark_leaf, const char *log_tag,
                                const char *step) {
  char path[MAX_PATH] = {};
  FILE *f = nullptr;
  if (app::detail::exe_sidecar_path_a(path, MAX_PATH, mark_leaf) &&
      fopen_s(&f, path, "a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fclose(f);
  }
  LOGGING(LOG_INFO, "%s: %s", log_tag, step);
  std::fprintf(stderr, "%s: %s\n", log_tag, step);
  std::fflush(stderr);
}

inline LRESULT CALLBACK showcase_blank_wnd_proc(HWND hwnd, UINT msg,
                                                WPARAM wparam, LPARAM lparam) {
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// Registers |class_name| (idempotent) and creates a WS_POPUP of
// |width|x|height|.
inline HWND create_showcase_popup(const wchar_t *class_name,
                                  const wchar_t *title, int width, int height) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = showcase_blank_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = class_name;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return nullptr;
  }
  return CreateWindowExW(0, class_name, title, WS_POPUP, 0, 0, width, height,
                         nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

} // namespace legacy_app

#endif // LEGACY_APP_SHELL_SHOWCASE_HOST_H_
