// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_TREE_VIEW_H_
#define UI_VIEWS_PRIMITIVES_TREE_VIEW_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

class ScrollView;

// Hierarchical rows with optional checkbox, selection, and expand/collapse.
// Empty |parent_id| inserts a root. Paints with Theme; scrolls via ScrollView.
class TreeView : public View {
 public:
  using NodeId = std::string;
  using SelectionChanged = std::function<void(const NodeId&)>;
  using CheckedChanged = std::function<void(const NodeId&, bool)>;
  using ContextRequested = std::function<void(const NodeId&, Point screen)>;

  TreeView();
  ~TreeView() override;

  void clear();
  void add_node(const NodeId& parent_id, NodeId id, std::string label,
                bool checked);

  void set_selection_changed(SelectionChanged fn);
  void set_checked_changed(CheckedChanged fn);
  void set_context_requested(ContextRequested fn);
  const NodeId& selected_id() const { return selected_id_; }

  void layout() override;
  bool on_mouse_event(const MouseEvent& event) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  struct Node {
    NodeId id;
    NodeId parent_id;
    std::string label;
    bool checked = false;
    bool expanded = true;
    std::vector<NodeId> child_ids;
  };

  struct VisibleRow {
    NodeId id;
    int depth = 0;
  };

  class Rows;

  void rebuild_visible();
  void update_content_size();
  int row_at_point(int x, int y) const;
  Rect row_rect(int i) const;
  void select_id(const NodeId& id);
  void toggle_check(const NodeId& id);
  void toggle_expand(const NodeId& id);
  Point to_screen(int x, int y) const;
  void paint_rows(render::skia::Canvas* canvas);

  ScrollView* scroll_ = nullptr;
  Rows* rows_ = nullptr;
  std::map<NodeId, Node> nodes_;
  std::vector<NodeId> roots_;
  std::vector<VisibleRow> visible_;
  NodeId selected_id_;
  SelectionChanged selection_changed_;
  CheckedChanged checked_changed_;
  ContextRequested context_requested_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_TREE_VIEW_H_
