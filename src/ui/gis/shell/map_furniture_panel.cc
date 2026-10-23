// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/map_furniture_panel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/checkbox.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

constexpr int k_panel_w = 260;
constexpr int k_panel_h = 160;

}  // namespace

MapFurniturePanel::MapFurniturePanel() {
  MarkupRoot loaded = load_markup("shell/map_furniture_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({k_panel_w, k_panel_h});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  scale_bar_ = loaded.ids.find_as<Checkbox>("scale");
  north_arrow_ = loaded.ids.find_as<Checkbox>("north");
  border_ = loaded.ids.find_as<Checkbox>("border");
  eagle_eye_ = loaded.ids.find_as<Checkbox>("eagle");

  const Theme& theme = Theme::current();
  if (title_) {
    title_->set_color(theme.text_bright);
  }

  if (scale_bar_) {
    scale_bar_->set_checked(true);
    scale_bar_->set_change([this](bool on) {
      if (scale_bar_change_) {
        scale_bar_change_(on);
      }
    });
  }
  if (north_arrow_) {
    north_arrow_->set_checked(true);
    north_arrow_->set_change([this](bool on) {
      if (north_arrow_change_) {
        north_arrow_change_(on);
      }
    });
  }
  if (border_) {
    border_->set_checked(true);
    border_->set_change([this](bool on) {
      if (border_change_) {
        border_change_(on);
      }
    });
  }
  if (eagle_eye_) {
    eagle_eye_->set_checked(false);
    eagle_eye_->set_enabled(false);
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({k_panel_w, k_panel_h});
  add_child(std::move(loaded.root));
  set_preferred_size({k_panel_w, k_panel_h});
}

MapFurniturePanel::~MapFurniturePanel() {
  if (scale_bar_) {
    scale_bar_->set_change({});
  }
  if (north_arrow_) {
    north_arrow_->set_change({});
  }
  if (border_) {
    border_->set_change({});
  }
  remove_all_children();
  title_ = nullptr;
  scale_bar_ = nullptr;
  north_arrow_ = nullptr;
  border_ = nullptr;
  eagle_eye_ = nullptr;
}

void MapFurniturePanel::set_scale_bar_checked(bool on) {
  if (scale_bar_) {
    scale_bar_->set_checked(on);
  }
}

void MapFurniturePanel::set_north_arrow_checked(bool on) {
  if (north_arrow_) {
    north_arrow_->set_checked(on);
  }
}

void MapFurniturePanel::set_border_checked(bool on) {
  if (border_) {
    border_->set_checked(on);
  }
}

bool MapFurniturePanel::scale_bar_checked() const {
  return scale_bar_ && scale_bar_->is_checked();
}

bool MapFurniturePanel::north_arrow_checked() const {
  return north_arrow_ && north_arrow_->is_checked();
}

bool MapFurniturePanel::border_checked() const {
  return border_ && border_->is_checked();
}

bool MapFurniturePanel::eagle_eye_enabled() const {
  return eagle_eye_ && eagle_eye_->is_enabled();
}

void MapFurniturePanel::set_scale_bar_change(std::function<void(bool)> fn) {
  scale_bar_change_ = std::move(fn);
}

void MapFurniturePanel::set_north_arrow_change(std::function<void(bool)> fn) {
  north_arrow_change_ = std::move(fn);
}

void MapFurniturePanel::set_border_change(std::function<void(bool)> fn) {
  border_change_ = std::move(fn);
}

void MapFurniturePanel::on_device_scale_factor_changed(float old_scale,
                                                      float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(k_panel_w, s), dip_to_px(k_panel_h, s)});
}

void MapFurniturePanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);

  const float scale = widget() ? widget()->device_scale_factor() : 1.f;
  const int hair = std::max(1, dip_to_px(1, scale));
  if (title_ && title_->is_visible()) {
    const Rect& h = title_->bounds();
    canvas->fill_rect(h.x, h.y, std::max(h.width, b.right() - h.x), h.height,
                      t.panel_header);
    canvas->fill_rect(h.x, h.y + h.height, std::max(h.width, b.right() - h.x),
                      hair, t.control_fill);
  }
}

}  // namespace views
}  // namespace ui
