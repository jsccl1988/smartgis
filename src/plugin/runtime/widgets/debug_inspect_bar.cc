// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/debug_inspect_bar.h"

#include <memory>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/label.h"

namespace plugin {
namespace {

void show_debug(content::PluginHost* host, int tab) {
  if (ShellUiSink* ui = shell_ui(host)) {
    (void)ui->show_debug_tab(tab);
  }
}

void show_inspect(content::PluginHost* host, const char* panel) {
  if (ShellUiSink* ui = shell_ui(host)) {
    (void)ui->show_inspect(panel);
  }
}

}  // namespace

DebugInspectBar::DebugInspectBar(content::PluginHost* host) {
  auto box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kHorizontal);
  box->set_inside_border(4, 4, 4, 4);
  box->set_between_child_spacing(8);
  set_layout_manager(std::move(box));

  add_child(std::make_unique<ui::views::Label>("Tools:"));
  auto mk = [host](const char* title, auto on_click) {
    auto btn = std::make_unique<ui::views::Button>(title);
    btn->set_click(std::move(on_click));
    return btn;
  };
  add_child(mk("Debug", [host]() { show_debug(host, 1); }));
  add_child(mk("Inspect", [host]() { show_inspect(host, "measure"); }));
  add_child(mk("Trace", [host]() { show_debug(host, 2); }));
  set_preferred_size({480, 28});
}

}  // namespace plugin
