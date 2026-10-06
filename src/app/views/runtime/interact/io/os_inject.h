// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_IO_OS_INJECT_H_
#define APP_VIEWS_RUNTIME_INTERACT_IO_OS_INJECT_H_

#include <windows.h>

#include <string>
#include <vector>

#include "app/views/runtime/interact/wire/ast.h"

namespace app {
namespace detail {

LPARAM client_lparam(int x, int y);

bool post_mouse(HWND hwnd, UINT down, UINT up, int x, int y);

bool post_drag(HWND hwnd, int x0, int y0, int x1, int y1);

bool post_wheel(HWND hwnd, int x, int y, int delta);

bool post_path(HWND hwnd, const std::vector<Point>& pts);

WORD vk_from_name(const std::string& name);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_IO_OS_INJECT_H_
