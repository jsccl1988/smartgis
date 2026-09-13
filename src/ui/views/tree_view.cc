// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/tree_view.h"

#include <algorithm>

#include "render/skia/canvas.h"
#include "ui/views/scroll_view.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kRowHeight = 20;
constexpr int kDepthIndent = 16;
constexpr int kTwistyW = 14;
constexpr int kCheckW = 16;

}  // namespace

// Content surface inside the ScrollView; paints flattened visible rows.
class TreeView::Rows : public View {
 public:
  explicit Rows(TreeView* host) : host_(host) {}

 protected:
  void paint_self(render::skia::Canvas* canvas) override {
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
  rows_->set_preferred_size({w, static_cast<int>(visible_.size()) * kRowHeight});
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
  const int scroll = scroll_ ? scroll_->scroll_offset() : 0;
  const int local = y - bounds().y + scroll;
  if (local < 0) {
    return -1;
  }
  const int i = local / kRowHeight;
  if (i < 0 || i >= static_cast<int>(visible_.size())) {
    return -1;
  }
  return i;
}

Rect TreeView::row_rect(int i) const {
  const int scroll = scroll_ ? scroll_->scroll_offset() : 0;
  return {bounds().x, bounds().y + i * kRowHeight - scroll, bounds().width,
          kRowHeight};
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
    return View::on_mouse_event(event);
  }
  const VisibleRow& row = visible_[static_cast<size_t>(i)];
  const Rect r = row_rect(i);
  const int indent = r.x + row.depth * kDepthIndent;
  const int twisty_x1 = indent + kTwistyW;
  const int check_x0 = indent + kTwistyW;
  const int check_x1 = check_x0 + kCheckW;

  if (event.type == MouseEvent::Type::kUp && event.button == 2) {
    select_id(row.id);
    if (context_requested_) {
      context_requested_(row.id, to_screen(event.x, event.y));
    }
    return true;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
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

void TreeView::paint_self(render::skia::Canvas* canvas) {
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

void TreeView::paint_rows(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
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
    const int indent = r.x + row.depth * kDepthIndent;
    if (!it->second.child_ids.empty()) {
      const wchar_t* mark = it->second.expanded ? L"-" : L"+";
      canvas->draw_text(indent + 2, r.y + 3, mark, t.text_bright);
    }
    const render::skia::Color box =
        it->second.checked ? t.accent : t.control_unchecked;
    canvas->fill_rect(indent + kTwistyW, r.y + 2, kCheckW, kCheckW, box);
    const std::wstring w = utf8_to_wide(it->second.label);
    canvas->draw_text(indent + kTwistyW + kCheckW + 6, r.y + 3, w.c_str(),
                      t.text);
  }
}

}  // namespace views
}  // namespace ui
