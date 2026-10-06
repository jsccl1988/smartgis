// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/shell_ui.h"

namespace plugin {

void ShellUiSink::set_bridges(MountInspectorFn mount, ShowDebugTabFn debug_tab,
                              ShowInspectFn inspect) {
  mount_ = std::move(mount);
  show_debug_ = std::move(debug_tab);
  show_inspect_ = std::move(inspect);
}

}  // namespace plugin
