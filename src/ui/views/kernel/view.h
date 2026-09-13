// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_VIEW_H_
#define UI_VIEWS_VIEW_H_

#include <memory>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/event.h"

namespace render {
namespace skia {
class Canvas;
}
}  // namespace render

namespace ui {
namespace views {

class LayoutManager;
class Widget;

struct Point {
  int x = 0;
  int y = 0;
};

struct Size {
  int width = 0;
  int height = 0;
};

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  int right() const { return x + width; }
  int bottom() const { return y + height; }
  bool contains(int px, int py) const {
    return px >= x && py >= y && px < right() && py < bottom();
  }
};

// Retained-mode node. Children are owned. Optional native HWND (map host).
class View {
 public:
  View();
  virtual ~View();

  View(const View&) = delete;
  View& operator=(const View&) = delete;

  void add_child(std::unique_ptr<View> child);
  void remove_all_children();
  View* child_at(size_t i) const;
  size_t child_count() const { return children_.size(); }
  View* parent() const { return parent_; }

  void set_bounds(const Rect& bounds);
  const Rect& bounds() const { return bounds_; }

  void set_preferred_size(const Size& size) { preferred_size_ = size; }
  const Size& preferred_size() const { return preferred_size_; }

  void set_layout_manager(std::unique_ptr<LayoutManager> layout);
  LayoutManager* layout_manager() const { return layout_.get(); }

  Widget* widget() const { return widget_; }
  void set_widget(Widget* widget);

  HWND native_view() const { return native_hwnd_; }

  void set_visible(bool visible);
  bool is_visible() const;
  void set_enabled(bool enabled);
  bool is_enabled() const;
  void set_focusable(bool focusable);
  bool is_focusable() const { return focusable_; }
  bool is_focused() const;
  bool request_focus();

  bool is_hovered() const { return hovered_; }
  bool is_pressed() const { return pressed_; }
  void set_hovered(bool hovered);
  void set_pressed(bool pressed);

  void invalidate();
  void schedule_paint();

  virtual void layout();
  virtual void paint(render::skia::Canvas* canvas);
  virtual bool on_mouse_event(const MouseEvent& event);
  virtual bool on_key_event(const KeyEvent& event);
  virtual bool on_char_event(const CharEvent& event);
  virtual void on_focus();
  virtual void on_blur();

  View* get_view_at(int x, int y);

  // Create/move a child HWND when this view hosts native content.
  void realize_native();
  void realize_native_tree();
  void sync_native_bounds();
  void sync_native_tree();

 protected:
  virtual HWND create_native_view(HWND parent);
  virtual void paint_self(render::skia::Canvas* canvas);

  void set_native_view(HWND hwnd) { native_hwnd_ = hwnd; }

 private:
  View* parent_ = nullptr;
  Widget* widget_ = nullptr;
  Rect bounds_;
  Size preferred_size_;
  HWND native_hwnd_ = nullptr;
  std::unique_ptr<LayoutManager> layout_;
  std::vector<std::unique_ptr<View>> children_;
  bool visible_ = true;
  bool enabled_ = true;
  bool focusable_ = false;
  bool hovered_ = false;
  bool pressed_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_VIEW_H_
