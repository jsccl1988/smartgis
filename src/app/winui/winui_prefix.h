// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_WINUI_PREFIX_H_
#define APP_WINUI_WINUI_PREFIX_H_

// WinUI / C++/WinRT needs a current NTDDI (IWindowNative, dxinterop).
// Repo-wide winver expands to NTDDI 0x0A000000 (below RS4).
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0A00000C

#include <unknwn.h>
#include <windows.h>
#undef GetCurrentTime

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#endif  // APP_WINUI_WINUI_PREFIX_H_
