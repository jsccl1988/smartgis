// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_SLIDER_H_
#define UI_VIEWS_PRIMITIVES_SLIDER_H_

#include <functional>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// Horizontal value scrubber. Value is clamped to [min, max]; drag or click
// moves the thumb. Used by AtmospherePanel time scrub and similar chrome.
class Slider : public View {
 public:
  Slider();

  void set_range(double min_value, double max_value);
  double min_value() const { return min_; }
  double max_value() const { return max_; }

  void set_value(double value);
  double value() const { return value_; }

  void set_change(std::function<void(double)> fn);

  bool on_mouse_event(const MouseEvent& e) override;
  bool on_key_event(const KeyEvent& e) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void set_value_from_x(int local_x, bool notify);
  int thumb_center_x() const;

  double min_ = 0.0;
  double max_ = 1.0;
  double value_ = 0.0;
  bool dragging_ = false;
  std::function<void(double)> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_SLIDER_H_
