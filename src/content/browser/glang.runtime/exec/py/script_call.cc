// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/script_call.h"

namespace content {
namespace code {
namespace {

ScriptHandler* g_handler = nullptr;

}  // namespace

void set_script_handler(ScriptHandler* handler) {
  g_handler = handler;
}

void clear_script_handler() {
  g_handler = nullptr;
}

ScriptHandler* script_handler() {
  return g_handler;
}

}  // namespace code
}  // namespace content
