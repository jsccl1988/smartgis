// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/style/layer_properties_panel.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gis/style/symbology_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

LayerPropertiesPanel::LayerPropertiesPanel() {
  MarkupRoot loaded = load_markup("style/layer_properties_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({300, 260});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  View* tabs_host = loaded.ids.find("tabs_host");

  auto symbology = std::make_unique<SymbologyPanel>();
  symbology_ = symbology.get();

  auto source = std::make_unique<Label>("Source: (none)");
  source->set_preferred_size({260, 80});
  source_label_ = source.get();

  auto tabs = std::make_unique<TabStrip>();
  tabs->set_preferred_size({280, 220});
  tabs->add_tab("Symbology", std::move(symbology));
  tabs->add_tab("Source", std::move(source));
  tabs_ = tabs.get();

  if (tabs_host) {
    tabs_host->set_layout_manager(std::make_unique<FillLayout>());
    tabs_host->add_child(std::move(tabs));
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({300, 260});
  add_child(std::move(loaded.root));
  set_preferred_size({300, 260});
}

LayerPropertiesPanel::~LayerPropertiesPanel() {
  remove_all_children();
  title_ = nullptr;
  tabs_ = nullptr;
  symbology_ = nullptr;
  source_label_ = nullptr;
}

void LayerPropertiesPanel::set_source_text(std::string text) {
  source_text_ = std::move(text);
  if (source_label_) {
    source_label_->set_text(std::string("Source: ") +
                            (source_text_.empty() ? "(none)" : source_text_));
  }
}

void LayerPropertiesPanel::on_device_scale_factor_changed(float old_scale,
                                                          float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(300, s), dip_to_px(260, s)});
}

void LayerPropertiesPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
