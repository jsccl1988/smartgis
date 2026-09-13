// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_DETAIL_BOOTSTRAP_H_
#define APP_WINUI_DETAIL_BOOTSTRAP_H_

#include <windows.h>

namespace app {
namespace winui {
namespace detail {

// Unpackaged Win32 + Windows App SDK bootstrapper. Prints a human-readable
// error (same severity as missing MFC on build.bat app) and returns false
// when the runtime is not installed.
bool initialize_windows_app_sdk();
void shutdown_windows_app_sdk();

}  // namespace detail
}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_DETAIL_BOOTSTRAP_H_
