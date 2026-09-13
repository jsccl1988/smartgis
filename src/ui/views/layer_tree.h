// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_LAYER_TREE_H_
#define UI_VIEWS_LAYER_TREE_H_

#include <functional>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ui {
namespace views {

// Vertical layer list: name + visibility checkbox. IDs are opaque strings;
// this control never stores SmtLayer* or other GIS pointers.
class LayerTree : public View {
 public:
  using VisibleChanged = std::function<void(const std::string& id, bool visible)>;
  using SelectionChanged = std::function<void(const std::string& id)>;
  using ContextRequested =
      std::function<void(const std::string& id, Point screen)>;

  LayerTree();
  ~LayerTree() override;

  void clear();
  void add_layer(std::string id, std::string name, bool visible);

  void set_visible_changed(VisibleChanged fn);
  void set_selection_changed(SelectionChanged fn);
  void set_context_requested(ContextRequested fn);

  const std::string& selected_id() const { return selected_id_; }
  size_t layer_count() const { return rows_.size(); }

  // Layer on/off for |id|, not View::is_visible() (layout / paint / hit-test).
  bool is_layer_visible(const std::string& id) const;

  void layout() override;
  bool on_mouse_event(const MouseEvent& event) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  class LayerRow;

  friend class LayerRow;

  LayerRow* row_at(const std::string& id) const;
  LayerRow* row_at_point(int x, int y) const;
  void select_id(const std::string& id);
  void notify_visible(const std::string& id, bool visible);
  bool handle_row_mouse(LayerRow* row, const MouseEvent& event);
  Point to_screen(int x, int y) const;

  std::vector<LayerRow*> rows_;
  std::string selected_id_;
  VisibleChanged visible_changed_;
  SelectionChanged selection_changed_;
  ContextRequested context_requested_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_LAYER_TREE_H_
