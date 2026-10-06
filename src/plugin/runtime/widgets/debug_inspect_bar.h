// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_DEBUG_INSPECT_BAR_H_
#define PLUGIN_WIDGETS_DEBUG_INSPECT_BAR_H_

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Compact Debug / Inspect / Trace buttons that drive chrome Diagnostic Tools
// and inspector tabs via plugin.ui.shell.
class PLUGIN_HOST_EXPORT DebugInspectBar : public ui::views::View {
 public:
  explicit DebugInspectBar(content::PluginHost* host);
};

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_DEBUG_INSPECT_BAR_H_
