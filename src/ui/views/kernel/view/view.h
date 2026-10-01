// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_VIEW_VIEW_H_
#define UI_VIEWS_KERNEL_VIEW_VIEW_H_

#include "ui/ui_export.h"
#include <memory>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/display_list/display_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/kernel/shell/event.h"

namespace ui {
namespace gfx {
class Canvas;
}
namespace views {

class LayoutManager;
class PaintDelegate;
class Widget;

// Geometry types live in ui::gfx; re-exported here so existing views call
// sites keep unqualified Point / Size / Rect.
using Point = ui::gfx::Point;
using Size = ui::gfx::Size;
using Rect = ui::gfx::Rect;

// Retained-mode node. Children are owned. Optional native HWND (map host).
class UI_EXPORT View {
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

  void set_preferred_size(const Size& size);
  // Stored hint (leaves, Splitter seeds). Prefer get_preferred_size() when a
  // LayoutManager can derive size from children.
  const Size& preferred_size() const { return preferred_size_; }
  // Layout-aware preferred size: asks LayoutManager when present.
  Size get_preferred_size() const;

  void set_layout_manager(std::unique_ptr<LayoutManager> layout);
  LayoutManager* layout_manager() const { return layout_.get(); }

  Widget* widget() const { return widget_; }
  void set_widget(Widget* widget);

  HWND native_view() const { return native_hwnd_; }

  void set_visible(bool visible);
  // Own flag plus every ancestor. Layout hoists the ancestor walk once.
  bool is_visible() const;
  // This node only. Does not walk parents.
  bool is_locally_visible() const { return visible_; }
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
  // Bump the display list so the next paint re-records paint_self.
  void invalidate_commands();
  void mark_needs_layout();
  bool needs_layout() const { return needs_layout_; }

  // Non-owning. Caller must clear or outlive this View.
  void set_paint_delegate(PaintDelegate* delegate);
  PaintDelegate* paint_delegate() const { return paint_delegate_; }

  // Used by builtin RoleForwardPainter to invoke protected paint_self.
  void paint_contents_for_painter(ui::gfx::Canvas* canvas);

  // ScrollView writes the viewport in the same coordinate space as bounds.
  // Empty means "no hint" (the view uses the canvas clip or its own bounds).
  void set_exposed_rect(const Rect& rect);
  const Rect& exposed_rect() const { return exposed_; }

  // Keep layout/paint/event slots at stable vtable offsets across the
  // ui_views PE and consumer EXEs. Append new virtuals below — never insert
  // above.
  virtual void layout();
  virtual void paint(ui::gfx::Canvas* canvas);
  // Records dirty DisplayLists (UI/recording thread only), then appends a
  // copy of this subtree's commands into |out| for Commit. When
  // |dirty_or_null| is non-null and non-empty, skips this node and its
  // subtree if bounds do not intersect the dirty rect.
  void append_commands_to(ui::gfx::DisplayList* out,
                          const Rect* dirty_or_null = nullptr);
  virtual bool on_mouse_event(const MouseEvent& event);
  virtual bool on_key_event(const KeyEvent& event);
  virtual bool on_char_event(const CharEvent& event);
  virtual void on_focus();
  virtual void on_blur();

  // Called when the host Widget DPI scale changes. Default multiplies
  // preferred_size by new/old. Text controls should remeasure instead.
  virtual void on_device_scale_factor_changed(float old_scale, float new_scale);

  // Walk this subtree, invoking on_device_scale_factor_changed on each node.
  void propagate_device_scale_factor_changed(float old_scale, float new_scale);

  // When true, layout_check skips "child-outside-parent" for this node's
  // children (ScrollView content / Combobox dropdown overlay).
  virtual bool allows_child_overflow() const { return false; }

  // Stable type key for PainterRegistry (e.g. "button"). Empty = no registry
  // lookup; subclass paint_self runs instead. Appended after legacy virtuals
  // so cross-DLL View subclasses keep layout/paint slots.
  virtual std::string_view paint_role() const;

  View* get_view_at(int x, int y);

  // Create/move a child HWND when this view hosts native content.
  void realize_native();
  void realize_native_tree();
  void sync_native_bounds();
  void sync_native_tree();

 protected:
  virtual HWND create_native_view(HWND parent);
  virtual void paint_self(ui::gfx::Canvas* canvas);
  // Re-records paint_self into commands_ when dirty. Uses the thread_local
  // DisplayList recorder; safe on the UI/recording thread only.
  void ensure_commands_recorded();
  // Records paint_self when inputs changed, then replays into |canvas|.
  void paint_commands(ui::gfx::Canvas* canvas);

  // Clears needs_layout and suppresses ancestor marks for this pass.
  class LayoutScope {
   public:
    explicit LayoutScope(View* view) : view_(view) {
      view_->needs_layout_ = false;
      view_->in_layout_ = true;
    }
    ~LayoutScope() { view_->in_layout_ = false; }
    LayoutScope(const LayoutScope&) = delete;
    LayoutScope& operator=(const LayoutScope&) = delete;

   private:
    View* view_;
  };

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
  bool needs_layout_ = true;
  bool in_layout_ = false;
  bool commands_dirty_ = true;
  bool commands_ready_ = false;
  Rect exposed_{};
  ui::gfx::DisplayList commands_;
  PaintDelegate* paint_delegate_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_VIEW_VIEW_H_
