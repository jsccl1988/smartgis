// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_EVAL_HOST_H_
#define IL_RUNTIME_CODEGEN_EVAL_HOST_H_

#include "app/views/il.runtime/frontend/ast.h"
#include "content/browser/capability/host.h"
#include "content/public/map_layer_types.h"

namespace app {
namespace detail {

void host_mark(content::CapabilityHost& host, const char* token);

void host_pump(content::CapabilityHost& host, int ms);

bool dispatch_edit(content::CapabilityHost& host, const content::InputEvent& e);

bool driver_ok(DriverFilter f);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_EVAL_HOST_H_
