// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/host/host.h"

namespace app {
namespace detail {

void host_mark(content::CapabilityHost& host, const char* token) {
  if (host.mark) {
    host.mark(token ? token : "");
  }
}

void host_pump(content::CapabilityHost& host, int ms) {
  if (host.pump) {
    host.pump(ms);
  }
}

HWND host_hwnd(content::CapabilityHost& host) {
  if (!host.shell_hwnd) {
    return nullptr;
  }
  return reinterpret_cast<HWND>(host.shell_hwnd());
}

bool dispatch_edit(content::CapabilityHost& host, const content::InputEvent& e) {
  return host.dispatch_edit_input && host.dispatch_edit_input(e);
}

bool driver_ok(DriverFilter f) {
  // Inproc executor: skip OS-only steps.
  if (f == DriverFilter::kOs) {
    return false;
  }
  return true;
}

}  // namespace detail
}  // namespace app
