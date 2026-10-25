// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/debug_inspect_bar.h"

#include <functional>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"
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

ui::views::Button* make_tool_button(const char* title,
                                    std::function<void()> on_click) {
  auto* btn = new ui::views::Button(title);
  btn->set_click(std::move(on_click));
  return btn;
}

}  // namespace

DebugInspectBar::DebugInspectBar(content::PluginHost* host) {
  ui::views::BoxLayout* box = ui::views::install_box_layout(
      this, ui::views::BoxLayout::Orientation::kHorizontal);
  if (box) {
    box->set_inside_border(4, 4, 4, 4);
    box->set_between_child_spacing(8);
  }

  ui::views::view_adopt_child(this, new ui::views::Label("Tools:"));
  ui::views::view_adopt_child(
      this, make_tool_button("Debug", [host]() { show_debug(host, 1); }));
  ui::views::view_adopt_child(
      this,
      make_tool_button("Inspect", [host]() { show_inspect(host, "measure"); }));
  ui::views::view_adopt_child(
      this, make_tool_button("Trace", [host]() { show_debug(host, 2); }));
  set_preferred_size({480, 28});
}

}  // namespace plugin
