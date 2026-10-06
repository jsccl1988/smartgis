// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/present_surface_picker.h"

#include <memory>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/widgets/debug_inspect_bar.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"

namespace plugin {

PresentSurfacePicker::PresentSurfacePicker(content::PluginHost* host)
    : host_(host) {
  auto box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kHorizontal);
  box->set_inside_border(4, 4, 4, 4);
  box->set_between_child_spacing(8);
  auto label = std::make_unique<ui::views::Label>("Present:");
  auto combo = std::make_unique<ui::views::Combobox>();
  combo_ = combo.get();
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
  set_layout_manager(std::move(box));
  add_child(std::move(label));
  add_child(std::move(combo));
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
  auto root = std::make_unique<ui::views::View>();
  auto box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  auto picker = std::make_unique<PresentSurfacePicker>(host);
  auto tools = std::make_unique<DebugInspectBar>(host);
  ui::views::View* body_ptr = body.get();
  if (body_ptr) {
    box->set_flex_for_view(body_ptr, 1);
  }
  root->set_layout_manager(std::move(box));
  root->add_child(std::move(picker));
  root->add_child(std::move(tools));
  if (body) {
    const auto pref = body->preferred_size();
    root->set_preferred_size(
        {pref.width > 0 ? pref.width : 520, pref.height + 60});
    root->add_child(std::move(body));
  } else {
    root->set_preferred_size({480, 60});
  }
  return root;
}

}  // namespace plugin
