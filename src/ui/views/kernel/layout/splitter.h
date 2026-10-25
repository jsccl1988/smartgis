// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_SPLITTER_H_
#define UI_VIEWS_KERNEL_LAYOUT_SPLITTER_H_

#include "ui/ui_export.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class SplitterGrip;

// Two-pane host with a draggable SplitterGrip seam. Horizontal is left|right;
// vertical is top|bottom. Pane children (non-grip) [0] primary, [1] secondary.
// On host resize (before any user drag): a pane with preferred size 0 absorbs
// growth; a pane with positive preferred size stays fixed. After a drag, the
// split ratio is preserved when the host grows or shrinks.
class UI_EXPORT Splitter : public View {
 public:
  enum class Orientation { kHorizontal, kVertical };

  explicit Splitter(Orientation orientation);
  ~Splitter() override;

  void set_collapsed(bool collapsed);
  bool is_collapsed() const;

  // Clear the one-shot seed and lay out again from child preferred sizes.
  // When |force| is false, a user grip drag keeps the seam (geometry only).
  // |force| recovers stub Map/3D clients after OS resize / mis-hit grip drags
  // (ui.interact: present ~514x99 → black FlyCube export).
  void reseed(bool force = false);

  // Force SecondaryFixed with primary ≥ |min_primary_frac| of the host (default
  // 2/3). Use after Diagnostic/Inspector re-opens so a Proportional seed cannot
  // leave Map/3D on a stub client.
  void enforce_primary_floor(float min_primary_frac = 2.f / 3.f);

  // Seam thickness (layout gap + grip hit target). Slightly wider than the
  // old 5px paint-only bar so Map HWND edges do not steal the first click.
  static constexpr int kBarPx = 8;

  void layout() override;
  // Forwards to children so SplitterGrip (topmost) receives the seam hit.
  bool on_mouse_event(const MouseEvent& event) override;
  void on_device_scale_factor_changed(float old_scale, float new_scale) override;
  std::string_view paint_role() const override;

  // Used by SplitterGrip.
  bool is_horizontal_split() const;
  bool is_grip_dragging() const;
  bool on_grip_mouse(const MouseEvent& event);

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  friend class SplitterGrip;

  enum class ResizePolicy {
    kPrimaryFixed,
    kSecondaryFixed,
    kProportional,
  };

  static constexpr int kMinPanePx = 40;

  bool is_horizontal() const { return is_horizontal_split(); }
  int main_extent() const;
  Rect bar_rect() const;
  View* pane_at(size_t index) const;
  size_t pane_count() const;
  void ensure_grip();
  void seed_split_if_needed();
  void adjust_for_host_resize();
  void clamp_primary();
  void apply_child_bounds();
  void begin_drag(int pos);
  void update_drag(int pos);
  void end_drag();

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
  // Last so in-DLL helpers own the pointer; panes stay ahead in child order.
  SplitterGrip* grip_ = nullptr;
};

UI_EXPORT std::unique_ptr<Splitter> make_splitter(Splitter::Orientation orientation);
UI_EXPORT Splitter* splitter_new(Splitter::Orientation orientation);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_SPLITTER_H_
