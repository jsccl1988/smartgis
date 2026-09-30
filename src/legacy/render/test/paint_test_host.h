// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_TEST_PAINT_TEST_HOST_H_
#define LEGACY_RENDER_TEST_PAINT_TEST_HOST_H_

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace legacy_render {
namespace detail {

// Directory of the running module with a trailing slash, or empty on failure.
inline std::string exe_dir_with_slash() {
  char path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i + 1] = '\0';
      break;
    }
  }
  return path;
}

// Resolves a China vector sample next to the exe or under testing/data.
inline std::string find_china_vector_sample() {
  const std::string dir = exe_dir_with_slash();
  if (dir.empty()) {
    return {};
  }
  static constexpr const char* kRel[] = {
      "..\\data\\china_city.gpkg",
      "..\\data\\china_city.geojson",
      "..\\data\\china_plp.geojson",
      "data\\china_city.gpkg",
      "data\\china_city.geojson",
      "data\\china_plp.geojson",
      "china_city.gpkg",
      "china_city.geojson",
      "china_plp.geojson",
      "testing\\data\\china_city.gpkg",
      "testing\\data\\china_city.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_city.gpkg",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_city.gpkg",
      "..\\..\\testing\\data\\china_plp.geojson",
  };
  for (const char* r : kRel) {
    const std::string cand = dir + r;
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

// Empty client paint so DWM / STATIC do not wipe a direct BitBlt.
inline LRESULT CALLBACK paint_test_blank_wnd_proc(HWND hwnd, UINT msg,
                                                  WPARAM wparam,
                                                  LPARAM lparam) {
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

inline HWND create_paint_test_popup(const wchar_t* class_name,
                                    const wchar_t* title, int width,
                                    int height) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = paint_test_blank_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = class_name;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return nullptr;
  }
  return CreateWindowExW(0, class_name, title, WS_POPUP, 0, 0, width, height,
                         nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

}  // namespace detail
}  // namespace legacy_render

#endif  // LEGACY_RENDER_TEST_PAINT_TEST_HOST_H_
