// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/layer_tree.h"

#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {

namespace {

constexpr int kRowH = 28;
constexpr int kCheckSize = 16;
constexpr int kCheckPad = 6;

}  // namespace

// One hittable layer row: checkbox on the left, name to the right.
class LayerTree::LayerRow : public View {
 public:
  LayerRow(std::string id, std::string name, bool visible)
      : id_(std::move(id)), name_(std::move(name)), visible_(visible) {
    set_preferred_size({200, kRowH});
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
    const Rect& b = bounds();
    const int cx = b.x + kCheckPad;
    const int cy = b.y + (kRowH - kCheckSize) / 2;
    return x >= cx && x < cx + kCheckSize && y >= cy && y < cy + kCheckSize;
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
  void paint_self(render::skia::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    if (selected_) {
      canvas->fill_rect(b.x, b.y, b.width, b.height,
                        render::skia::color_rgb(0, 90, 158));
    } else if (is_hovered() || is_pressed()) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_hover);
    }
    const int cy = b.y + (kRowH - kCheckSize) / 2;
    canvas->fill_rect(b.x + kCheckPad, cy, kCheckSize, kCheckSize,
                      visible_ ? t.accent : t.control_unchecked);
    if (is_focused()) {
      draw_focus_ring(canvas, {b.x + kCheckPad, cy, kCheckSize, kCheckSize});
    }
    const std::wstring w = utf8_to_wide(name_);
    canvas->draw_text(b.x + kCheckPad + kCheckSize + 8, b.y + 6, w.c_str(),
                      t.text_bright);
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
  layout();
  schedule_paint();
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
  const Rect& b = bounds();
  int y = b.y;
  for (LayerRow* row : rows_) {
    if (!row || !row->is_visible()) {
      continue;
    }
    row->set_bounds({b.x, y, b.width, kRowH});
    y += kRowH;
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

void LayerTree::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
