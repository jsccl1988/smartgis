// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_INTERACT_HOST_UTIL_H_
#define APP_VIEWS_SHELL_RUNTIME_INTERACT_HOST_UTIL_H_

#include <windows.h>

#include "app/views/shell/runtime/interact/ast.h"
#include "content/browser/capability/host.h"
#include "content/public/map_types.h"

namespace app {
namespace detail {

void host_mark(content::CapabilityHost& host, const char* token);

void host_pump(content::CapabilityHost& host, int ms);

HWND host_hwnd(content::CapabilityHost& host);

bool dispatch_edit(content::CapabilityHost& host, const content::InputEvent& e);

bool driver_ok(DriverFilter f);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_INTERACT_HOST_UTIL_H_
