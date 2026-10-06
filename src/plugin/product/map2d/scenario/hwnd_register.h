// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_HWND_REGISTER_H_
#define PLUGIN_MAP2D_SCENARIO_HWND_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

// HWND/BMP showcase commands (china / align / orthogrid / print). Linked via
// map2d_harness into the Views exe, not the native plugin DLL.
bool register_map2d_showcase(content::PluginHost* host);

int scenario_china(HarnessShell& browser);
int scenario_align(HarnessShell& browser);
int scenario_orthogrid(HarnessShell& browser);
int scenario_print(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_HWND_REGISTER_H_
