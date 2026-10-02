// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/layer_tree.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

namespace {

// DIPs — match TreeView: fixed px rows clip the 20 DIP shell font at high DPI.
constexpr int kRowHeightDip = 32;
constexpr int kCheckSizeDip = 18;
constexpr int kCheckPadDip = 8;
constexpr int kLabelGapDip = 10;

}  // namespace

// One hittable layer row: checkbox on the left, name to the right.
class LayerTree::LayerRow : public View {
 public:
  LayerRow(std::string id, std::string name, bool visible)
      : id_(std::move(id)), name_(std::move(name)), visible_(visible) {
    set_preferred_size({200, kRowHeightDip});
    set_focusable(true);
  }

  void reset(std::string id, std::string name, bool visible) {
    id_ = std::move(id);
    name_ = std::move(name);
    visible_ = visible;
    schedule_paint();
  }

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  bool is_layer_visible() const { return visible_; }

  void set_layer_visible(bool visible) {
    if (visible_ == visible) {
      return;
    }
    visible_ = visible;
    schedule_paint();
  }

  void toggle_visible() { set_layer_visible(!visible_); }

  bool hit_checkbox(int x, int y) const {
    if (!owner_) {
      return false;
    }
    const Rect& b = bounds();
    const int check = owner_->check_size();
    const int pad = owner_->check_pad();
    const int row_h = owner_->row_height();
    const int cx = b.x + pad;
    const int cy = b.y + (row_h - check) / 2;
    return x >= cx && x < cx + check && y >= cy && y < cy + check;
  }

  void set_owner(LayerTree* owner) { owner_ = owner; }

  void set_selected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    schedule_paint();
  }

  bool on_mouse_event(const MouseEvent& event) override {
    return owner_ ? owner_->handle_row_mouse(this, event) : false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !owner_) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const int row_h = owner_->row_height();
    const int check = owner_->check_size();
    const int pad = owner_->check_pad();
    const float scale = owner_->scale_factor();
    // Always paint a row plate so labels stay readable on dark chrome.
    // Selected: quiet hover fill + thin accent rail (not full-width saturated
    // blue that steals hierarchy from active TabStrip cells).
    if (selected_) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_hover);
      const int rail = std::max(2, dip_to_px(3, scale));
      canvas->fill_rect(b.x, b.y, rail, b.height, t.accent);
    } else if (is_hovered() || is_pressed()) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_hover);
    } else {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_fill);
    }
    const int cy = b.y + (row_h - check) / 2;
    canvas->fill_rect(b.x + pad, cy, check, check,
                      visible_ ? t.accent : t.control_unchecked);
    if (is_focused()) {
      draw_focus_ring(canvas, {b.x + pad, cy, check, check});
    }
    const std::wstring w = utf8_to_wide(name_);
    const int text_x = b.x + pad + check + dip_to_px(kLabelGapDip, scale);
    const Size ink = measure_text_utf8(name_, scale);
    const int text_y = b.y + std::max(0, (row_h - ink.height) / 2);
    canvas->draw_text(text_x, text_y, w.c_str(), t.text_bright);
  }

 private:
  LayerTree* owner_ = nullptr;
  std::string id_;
  std::string name_;
  bool visible_ = true;
  bool selected_ = false;
};

LayerTree::LayerTree() {
  set_preferred_size({220, 200});
}

LayerTree::~LayerTree() = default;

float LayerTree::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int LayerTree::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

int LayerTree::check_size() const {
  return dip_to_px(kCheckSizeDip, scale_factor());
}

int LayerTree::check_pad() const {
  return dip_to_px(kCheckPadDip, scale_factor());
}

void LayerTree::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  layout();
  schedule_paint();
}

void LayerTree::clear() {
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* child = child_at(i)) {
      child->set_visible(false);
    }
  }
  rows_.clear();
  selected_id_.clear();
  schedule_paint();
}

void LayerTree::add_layer(std::string id, std::string name, bool visible) {
  LayerRow* row = nullptr;
  for (size_t i = 0; i < child_count(); ++i) {
    auto* existing = static_cast<LayerRow*>(child_at(i));
    if (!existing) {
      continue;
    }
    // is_visible() is ancestor-aware: a hidden Catalog tab would make every
    // row look free and overwrite the first layer. Reuse only unused rows.
    bool in_use = false;
    for (LayerRow* used : rows_) {
      if (used == existing) {
        in_use = true;
        break;
      }
    }
    if (!in_use) {
      existing->reset(std::move(id), std::move(name), visible);
      existing->set_visible(true);
      existing->set_selected(false);
      row = existing;
      break;
    }
  }
  if (!row) {
    auto created =
        std::make_unique<LayerRow>(std::move(id), std::move(name), visible);
    row = created.get();
    add_child(std::move(created));
  }
  row->set_owner(this);
  rows_.push_back(row);
  if (batch_layout_) {
    return;
  }
  layout();
  schedule_paint();
}

void LayerTree::set_layers(const std::vector<LayerDesc>& layers) {
  batch_layout_ = true;
  clear();
  std::string active_id;
  for (const LayerDesc& layer : layers) {
    if (layer.id.empty() && layer.name.empty()) {
      continue;
    }
    const std::string id = layer.id.empty() ? layer.name : layer.id;
    const std::string name = layer.name.empty() ? id : layer.name;
    add_layer(id, name, layer.visible);
    if (active_id.empty() && layer.active) {
      active_id = id;
    }
  }
  batch_layout_ = false;
  layout();
  schedule_paint();
  if (active_id.empty() && !rows_.empty() && rows_.front()) {
    active_id = rows_.front()->id();
  }
  // Silent select: clear() already wiped selected_id_, so select_id() would
  // always fire selection_changed_. After OGR replace that cascades into
  // CatalogCall + fill_attribute_rows over every MapLayer feature (AV / hang
  // under showcase SMT_SKIP_AMBOX_CATALOG). Host callers sync inspectors
  // explicitly when they need it (on_open / deferred China seed).
  if (!active_id.empty()) {
    selected_id_ = active_id;
    for (LayerRow* row : rows_) {
      if (row) {
        row->set_selected(row->id() == selected_id_);
      }
    }
  }
}

void LayerTree::select_layer(const std::string& id) {
  select_id(id);
}

void LayerTree::set_layer_visible(const std::string& id, bool visible) {
  if (LayerRow* row = row_at(id)) {
    row->set_layer_visible(visible);
  }
}

bool LayerTree::remove_layer(const std::string& id) {
  for (size_t i = 0; i < rows_.size(); ++i) {
    LayerRow* row = rows_[i];
    if (!row || row->id() != id) {
      continue;
    }
    row->set_visible(false);
    row->set_selected(false);
    rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(i));
    if (selected_id_ == id) {
      selected_id_.clear();
      if (!rows_.empty() && rows_.front()) {
        select_id(rows_.front()->id());
      }
    }
    layout();
    schedule_paint();
    return true;
  }
  return false;
}

bool LayerTree::move_layer(const std::string& id, int delta) {
  if (delta == 0 || rows_.size() < 2) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i] && rows_[i]->id() == id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return false;
  }
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(rows_.size())) {
    return false;
  }
  std::swap(rows_[static_cast<size_t>(index)],
            rows_[static_cast<size_t>(target)]);
  layout();
  schedule_paint();
  return true;
}

void LayerTree::set_visible_changed(VisibleChanged fn) {
  visible_changed_ = std::move(fn);
}

void LayerTree::set_selection_changed(SelectionChanged fn) {
  selection_changed_ = std::move(fn);
}

void LayerTree::set_context_requested(ContextRequested fn) {
  context_requested_ = std::move(fn);
}

bool LayerTree::layer_at(size_t index,
                         std::string* id,
                         std::string* name,
                         bool* visible) const {
  if (index >= rows_.size() || !rows_[index]) {
    return false;
  }
  const LayerRow* row = rows_[index];
  if (id) {
    *id = row->id();
  }
  if (name) {
    *name = row->name();
  }
  if (visible) {
    *visible = row->is_layer_visible();
  }
  return true;
}

bool LayerTree::is_layer_visible(const std::string& id) const {
  if (const LayerRow* row = row_at(id)) {
    return row->is_layer_visible();
  }
  return false;
}

LayerTree::LayerRow* LayerTree::row_at(const std::string& id) const {
  for (LayerRow* row : rows_) {
    if (row && row->id() == id) {
      return row;
    }
  }
  return nullptr;
}

LayerTree::LayerRow* LayerTree::row_at_point(int x, int y) const {
  for (LayerRow* row : rows_) {
    if (row && row->is_visible() && row->bounds().contains(x, y)) {
      return row;
    }
  }
  return nullptr;
}

void LayerTree::select_id(const std::string& id) {
  if (selected_id_ == id) {
    return;
  }
  selected_id_ = id;
  for (LayerRow* row : rows_) {
    if (row) {
      row->set_selected(row->id() == selected_id_);
    }
  }
  if (selection_changed_) {
    selection_changed_(selected_id_);
  }
}

void LayerTree::notify_visible(const std::string& id, bool visible) {
  if (visible_changed_) {
    visible_changed_(id, visible);
  }
}

Point LayerTree::to_screen(int x, int y) const {
  POINT p{x, y};
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd) {
    ClientToScreen(hwnd, &p);
  }
  return {p.x, p.y};
}

void LayerTree::layout() {
  ui::gfx::note_layout();
  LayoutScope scope(this);
  const Rect& b = bounds();
  const int row_h = row_height();
  int y = b.y;
  for (LayerRow* row : rows_) {
    if (!row) {
      continue;
    }
    // Keep rows inside the pane (layout_check / no paint overflow).
    if (y + row_h > b.bottom()) {
      row->set_visible(false);
      continue;
    }
    row->set_visible(true);
    row->set_preferred_size({b.width, row_h});
    row->set_bounds({b.x, y, b.width, row_h});
    y += row_h;
  }
}

bool LayerTree::handle_row_mouse(LayerRow* row, const MouseEvent& event) {
  if (!is_enabled() || !row) {
    return false;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 2) {
    select_id(row->id());
    if (context_requested_) {
      context_requested_(row->id(), to_screen(event.x, event.y));
    }
    return true;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
    if (row->hit_checkbox(event.x, event.y)) {
      row->toggle_visible();
      notify_visible(row->id(), row->is_layer_visible());
    }
    select_id(row->id());
    return true;
  }
  return event.type == MouseEvent::Type::kDown &&
         (event.button == 1 || event.button == 2);
}

bool LayerTree::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled()) {
    return false;
  }
  if (LayerRow* row = row_at_point(event.x, event.y)) {
    return handle_row_mouse(row, event);
  }
  return View::on_mouse_event(event);
}

void LayerTree::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
