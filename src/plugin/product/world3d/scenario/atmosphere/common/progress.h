// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PROGRESS_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PROGRESS_H_

namespace plugin {

class HarnessShell;

namespace detail {

void bind_atmosphere_showcase_shell(HarnessShell* shell);
HarnessShell* atmosphere_showcase_shell();
void atmosphere_showcase_mark(const char* step);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PROGRESS_H_
