// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/eval/eval_host.h"

#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {

void host_mark(content::CapabilityHost& host, const char* token) {
  ir::mark(host, token ? token : "");
}

void host_pump(content::CapabilityHost& host, int ms) {
  ir::pump(host, ms);
}

bool dispatch_edit(content::CapabilityHost& host, const content::InputEvent& e) {
  return ir::dispatch_edit(host, e);
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
