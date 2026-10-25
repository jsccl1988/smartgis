// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/present_surface_picker.h"

#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/widgets/debug_inspect_bar.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"

namespace plugin {

PresentSurfacePicker::PresentSurfacePicker(content::PluginHost* host)
    : host_(host) {
  ui::views::BoxLayout* box = ui::views::install_box_layout(
      this, ui::views::BoxLayout::Orientation::kHorizontal);
  if (box) {
    box->set_inside_border(4, 4, 4, 4);
    box->set_between_child_spacing(8);
  }
  // Children are new'd in this TU (plugin_host); register destroyers so the
  // shell can clear a picker owned by a product plugin without heap mismatch.
  auto* label = make_plugin_view<ui::views::Label>("Present:");
  auto* combo = make_plugin_view<ui::views::Combobox>();
  combo_ = combo;
  combo_->add_item("Main window");
  combo_->add_item("Map / World preview");
  const int surface = host_ ? host_->present_surface() : 0;
  combo_->set_selected_index(surface != 0 ? 1 : 0);
  combo_->set_change([this](int index) {
    const int surface = index != 0 ? 1 : 0;
    if (host_) {
      host_->set_present_surface(surface);
    }
    if (on_change_) {
      on_change_(surface);
    }
  });
  ui::views::view_adopt_child(this, label);
  ui::views::view_adopt_child(this, combo);
  set_preferred_size({480, 28});
}

void PresentSurfacePicker::set_on_surface_change(
    std::function<void(int surface)> fn) {
  on_change_ = std::move(fn);
}

int PresentSurfacePicker::surface() const {
  if (combo_) {
    return combo_->selected_index() != 0 ? 1 : 0;
  }
  return host_ ? host_->present_surface() : 0;
}

std::unique_ptr<ui::views::View> wrap_with_present_surface(
    content::PluginHost* host,
    std::unique_ptr<ui::views::View> body) {
  // Root / chrome allocated in plugin_host; body must already carry an
  // external destroyer from its allocating product plugin TU.
  auto* root = make_plugin_view<ui::views::View>();
  ui::views::BoxLayout* box = ui::views::install_box_layout(
      root, ui::views::BoxLayout::Orientation::kVertical);
  auto* picker = make_plugin_view<PresentSurfacePicker>(host);
  auto* tools = make_plugin_view<DebugInspectBar>(host);
  ui::views::View* body_ptr = body.get();
  if (box && body_ptr) {
    box->set_flex_for_view(body_ptr, 1);
  }
  ui::views::view_adopt_child(root, picker);
  ui::views::view_adopt_child(root, tools);
  if (body) {
    const auto pref = body->preferred_size();
    root->set_preferred_size(
        {pref.width > 0 ? pref.width : 520, pref.height + 60});
    ui::views::view_adopt_child(root, body.release());
  } else {
    root->set_preferred_size({480, 60});
  }
  return std::unique_ptr<ui::views::View>(root);
}

}  // namespace plugin
