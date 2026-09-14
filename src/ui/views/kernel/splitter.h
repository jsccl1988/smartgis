// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_SPLITTER_H_
#define UI_VIEWS_SPLITTER_H_

#include "ui/views/view.h"

namespace ui {
namespace views {

// Two-pane host with a 6px draggable bar. Horizontal is left|right; vertical
// is top|bottom. children[0] is primary, children[1] is secondary.
// On host resize (before any user drag): a pane with preferred size 0 absorbs
// growth; a pane with positive preferred size stays fixed. After a drag, the
// split ratio is preserved when the host grows or shrinks.
class Splitter : public View {
 public:
  enum class Orientation { kHorizontal, kVertical };

  explicit Splitter(Orientation orientation);

  void set_collapsed(bool collapsed);
  bool is_collapsed() const { return collapsed_; }

  void layout() override;
  bool on_mouse_event(const MouseEvent& event) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  enum class ResizePolicy {
    kPrimaryFixed,
    kSecondaryFixed,
    kProportional,
  };

  static constexpr int kBarPx = 6;
  static constexpr int kMinPanePx = 40;

  bool is_horizontal() const {
    return orientation_ == Orientation::kHorizontal;
  }
  int main_extent() const;
  Rect bar_rect() const;
  void seed_split_if_needed();
  void adjust_for_host_resize();
  void clamp_primary();
  void apply_child_bounds();
  void begin_drag(int pos);
  void update_drag(int pos);

  Orientation orientation_;
  ResizePolicy resize_policy_ = ResizePolicy::kProportional;
  int primary_extent_ = 0;
  int saved_primary_ = 0;
  int fixed_secondary_px_ = 0;
  int last_main_ = 0;
  bool split_seeded_ = false;
  bool user_adjusted_ = false;
  bool collapsed_ = false;
  bool dragging_ = false;
  int drag_origin_ = 0;
  int drag_primary_origin_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_SPLITTER_H_
