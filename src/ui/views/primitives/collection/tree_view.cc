// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/collection/tree_view.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/collection/scroll_view.h"

namespace ui {
namespace views {
namespace {

// DIPs — must scale with Widget::device_scale_factor. A fixed 20px row at 250%
// DPI (font ~45px) clips catalog labels so horizon looks "tiny".
constexpr int kRowHeightDip = 24;
constexpr int kDepthIndentDip = 16;
constexpr int kTwistyWDip = 14;
constexpr int kCheckWDip = 16;
constexpr int kTextPadYDip = 3;
constexpr int kCheckPadYDip = 2;

}  // namespace

// Content surface inside the ScrollView; paints flattened visible rows.
class TreeView::Rows : public View {
 public:
  explicit Rows(TreeView* host) : host_(host) {}

  bool on_mouse_event(const MouseEvent& event) override {
    // Deepest-hit dispatch lands here (child of ScrollView). Forward so
    // twisty/check/select run; do not bounce unhandled events back through
    // View::on_mouse_event (that would re-hit Rows and recurse).
    return host_ ? host_->on_mouse_event(event) : false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (host_) {
      host_->paint_rows(canvas);
    }
  }

 private:
  TreeView* host_ = nullptr;
};

TreeView::TreeView() {
  auto scroll = std::make_unique<ScrollView>();
  auto rows = std::make_unique<Rows>(this);
  rows_ = rows.get();
  scroll->add_child(std::move(rows));
  scroll_ = scroll.get();
  add_child(std::move(scroll));
  set_preferred_size({200, 160});
  set_focusable(true);
}

TreeView::~TreeView() = default;

float TreeView::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int TreeView::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

int TreeView::depth_indent() const {
  return dip_to_px(kDepthIndentDip, scale_factor());
}

int TreeView::twisty_width() const {
  return dip_to_px(kTwistyWDip, scale_factor());
}

int TreeView::check_width() const {
  return dip_to_px(kCheckWDip, scale_factor());
}

void TreeView::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  update_content_size();
  if (scroll_) {
    scroll_->layout();
  }
  schedule_paint();
}

void TreeView::clear() {
  nodes_.clear();
  roots_.clear();
  visible_.clear();
  selected_id_.clear();
  if (scroll_) {
    scroll_->set_scroll_offset(0);
  }
  update_content_size();
  schedule_paint();
}

void TreeView::add_node(const NodeId& parent_id, NodeId id, std::string label,
                        bool checked) {
  if (id.empty() || nodes_.contains(id)) {
    return;
  }
  Node node;
  node.id = std::move(id);
  node.parent_id = parent_id;
  node.label = std::move(label);
  node.checked = checked;
  if (!parent_id.empty()) {
    auto it = nodes_.find(parent_id);
    if (it != nodes_.end()) {
      it->second.child_ids.push_back(node.id);
    } else {
      roots_.push_back(node.id);
    }
  } else {
    roots_.push_back(node.id);
  }
  nodes_.emplace(node.id, std::move(node));
  rebuild_visible();
  update_content_size();
  schedule_paint();
}

void TreeView::set_selection_changed(SelectionChanged fn) {
  selection_changed_ = std::move(fn);
}

void TreeView::set_checked_changed(CheckedChanged fn) {
  checked_changed_ = std::move(fn);
}

void TreeView::set_context_requested(ContextRequested fn) {
  context_requested_ = std::move(fn);
}

void TreeView::rebuild_visible() {
  visible_.clear();
  const auto walk = [&](auto& self, const NodeId& id, int depth) -> void {
    const auto it = nodes_.find(id);
    if (it == nodes_.end()) {
      return;
    }
    visible_.push_back({id, depth});
    if (!it->second.expanded) {
      return;
    }
    for (const NodeId& child : it->second.child_ids) {
      self(self, child, depth + 1);
    }
  };
  for (const NodeId& id : roots_) {
    walk(walk, id, 0);
  }
}

void TreeView::update_content_size() {
  if (!rows_) {
    return;
  }
  const int w = std::max(bounds().width, 1);
  const int rh = row_height();
  rows_->set_preferred_size(
      {w, static_cast<int>(visible_.size()) * std::max(rh, 1)});
}

void TreeView::layout() {
  rebuild_visible();
  update_content_size();
  if (scroll_) {
    scroll_->set_bounds(bounds());
    scroll_->layout();
    scroll_->sync_native_bounds();
  }
}

int TreeView::row_at_point(int x, int y) const {
  if (!bounds().contains(x, y)) {
    return -1;
  }
  const int rh = row_height();
  if (rh <= 0) {
    return -1;
  }
  const int scroll = scroll_ ? scroll_->scroll_offset() : 0;
  const int local = y - bounds().y + scroll;
  if (local < 0) {
    return -1;
  }
  const int i = local / rh;
  if (i < 0 || i >= static_cast<int>(visible_.size())) {
    return -1;
  }
  return i;
}

Rect TreeView::row_rect(int i) const {
  const int rh = row_height();
  const int scroll = scroll_ ? scroll_->scroll_offset() : 0;
  return {bounds().x, bounds().y + i * rh - scroll, bounds().width, rh};
}

void TreeView::select_id(const NodeId& id) {
  if (selected_id_ == id) {
    return;
  }
  selected_id_ = id;
  schedule_paint();
  if (selection_changed_) {
    selection_changed_(selected_id_);
  }
}

void TreeView::toggle_check(const NodeId& id) {
  auto it = nodes_.find(id);
  if (it == nodes_.end()) {
    return;
  }
  it->second.checked = !it->second.checked;
  schedule_paint();
  if (checked_changed_) {
    checked_changed_(id, it->second.checked);
  }
}

void TreeView::toggle_expand(const NodeId& id) {
  auto it = nodes_.find(id);
  if (it == nodes_.end() || it->second.child_ids.empty()) {
    return;
  }
  it->second.expanded = !it->second.expanded;
  rebuild_visible();
  update_content_size();
  if (scroll_) {
    scroll_->layout();
  }
  schedule_paint();
}

Point TreeView::to_screen(int x, int y) const {
  Point screen{x, y};
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd) {
    POINT pt{x, y};
    ClientToScreen(hwnd, &pt);
    screen.x = pt.x;
    screen.y = pt.y;
  }
  return screen;
}

bool TreeView::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled() || !is_visible()) {
    return false;
  }
  if (event.type == MouseEvent::Type::kWheel) {
    return scroll_ ? scroll_->on_mouse_event(event) : false;
  }
  const int i = row_at_point(event.x, event.y);
  if (i < 0) {
    return false;
  }
  const VisibleRow& row = visible_[static_cast<size_t>(i)];
  const Rect r = row_rect(i);
  const int twisty_w = twisty_width();
  const int check_w = check_width();
  const int indent = r.x + row.depth * depth_indent();
  const int twisty_x1 = indent + twisty_w;
  const int check_x0 = indent + twisty_w;
  const int check_x1 = check_x0 + check_w;

  if (event.type == MouseEvent::Type::kUp && event.button == 2) {
    select_id(row.id);
    if (context_requested_) {
      context_requested_(row.id, to_screen(event.x, event.y));
    }
    return true;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
    request_focus();
    if (event.x < twisty_x1) {
      toggle_expand(row.id);
      select_id(row.id);
      return true;
    }
    if (event.x >= check_x0 && event.x < check_x1) {
      toggle_check(row.id);
      select_id(row.id);
      return true;
    }
    select_id(row.id);
    return true;
  }
  return event.type == MouseEvent::Type::kDown && event.button == 1;
}

void TreeView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
  if (is_focused()) {
    draw_focus_ring(canvas, b);
  }
}

void TreeView::paint_rows(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const float scale = scale_factor();
  const int twisty_w = twisty_width();
  const int check_w = check_width();
  const int indent_step = depth_indent();
  const int text_pad_y = dip_to_px(kTextPadYDip, scale);
  const int check_pad_y = dip_to_px(kCheckPadYDip, scale);
  const int label_gap = dip_to_px(6, scale);
  for (int i = 0; i < static_cast<int>(visible_.size()); ++i) {
    const VisibleRow& row = visible_[static_cast<size_t>(i)];
    const auto it = nodes_.find(row.id);
    if (it == nodes_.end()) {
      continue;
    }
    const Rect r = row_rect(i);
    if (r.bottom() < bounds().y || r.y > bounds().bottom()) {
      continue;
    }
    if (row.id == selected_id_) {
      canvas->fill_rect(r.x, r.y, r.width, r.height, t.accent);
    }
    const int indent = r.x + row.depth * indent_step;
    if (!it->second.child_ids.empty()) {
      const wchar_t* mark = it->second.expanded ? L"-" : L"+";
      canvas->draw_text(indent + dip_to_px(2, scale), r.y + text_pad_y, mark,
                        t.text_bright);
    }
    const ui::gfx::Color box =
        it->second.checked ? t.accent : t.control_unchecked;
    canvas->fill_rect(indent + twisty_w, r.y + check_pad_y, check_w, check_w,
                      box);
    const std::wstring w = utf8_to_wide(it->second.label);
    canvas->draw_text(indent + twisty_w + check_w + label_gap, r.y + text_pad_y,
                      w.c_str(), t.text);
  }
}

std::string_view TreeView::paint_role() const {
  return "tree_view";
}
}  // namespace views
}  // namespace ui
