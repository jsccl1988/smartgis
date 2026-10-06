// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_UI_SHELL_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_UI_SHELL_H_

#include <functional>
#include <memory>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/kernel/view/view.h"

namespace plugin {

// Chrome-installed inspector + Diagnostic Tools seams. Plugins mount docks and
// show Debug / Inspect / Trace without seeing BrowserView.
class ShellUiSink {
 public:
  using MountInspectorFn = std::function<bool(
      std::string_view dock_id, std::string_view title,
      std::unique_ptr<ui::views::View> page)>;
  // Diagnostic Tools tabs: 0 Output, 1 Console, 2 Trace, 3 Memory.
  using ShowDebugTabFn = std::function<bool(int tab)>;
  // Inspector pages: measure | selection | legend | layer | atmosphere | …
  using ShowInspectFn = std::function<bool(std::string_view panel)>;

  PLUGIN_HOST_EXPORT void set_bridges(MountInspectorFn mount,
                                      ShowDebugTabFn debug_tab,
                                      ShowInspectFn inspect);

  bool mount_inspector(std::string_view dock_id, std::string_view title,
                       std::unique_ptr<ui::views::View> page) const {
    return mount_ ? mount_(dock_id, title, std::move(page)) : false;
  }
  bool show_debug_tab(int tab) const {
    return show_debug_ ? show_debug_(tab) : false;
  }
  bool show_inspect(std::string_view panel) const {
    return show_inspect_ ? show_inspect_(panel) : false;
  }

 private:
  MountInspectorFn mount_;
  ShowDebugTabFn show_debug_;
  ShowInspectFn show_inspect_;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_UI_SHELL_H_
