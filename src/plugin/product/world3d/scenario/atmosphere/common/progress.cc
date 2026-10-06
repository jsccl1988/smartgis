// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"

#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {
namespace {

thread_local HarnessShell* g_shell = nullptr;

}  // namespace

void bind_atmosphere_scenario_shell(HarnessShell* shell) {
  g_shell = shell;
}

HarnessShell* atmosphere_scenario_shell() {
  return g_shell;
}

void atmosphere_mark(const char* step) {
  if (g_shell) {
    g_shell->mark_named(kMarkAtmosphere, step, false);
  }
}

}  // namespace detail
}  // namespace plugin
