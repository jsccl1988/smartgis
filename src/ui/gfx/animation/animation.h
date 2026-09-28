// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_ANIMATION_ANIMATION_H_
#define UI_GFX_ANIMATION_ANIMATION_H_

#include "ui/ui_export.h"
namespace ui {
namespace gfx {

// Minimal linear animation skeleton for shell chrome. Not a Chromium
// AnimationContainer / multi-curve stack. Hosts drive |step| with a 0..1
// progress value; this type does not own a timer thread.
class UI_EXPORT Animation {
 public:
  Animation() = default;
  virtual ~Animation();

  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;

  void start();
  void stop();
  bool is_animating() const { return animating_; }

  // Progress in [0, 1]. Clamped. Invokes on_progress.
  void step(double progress);

  double current_value() const { return value_; }

 protected:
  // Override to apply |value| (0..1) to chrome state.
  virtual void on_progress(double value);

 private:
  bool animating_ = false;
  double value_ = 0.0;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_ANIMATION_ANIMATION_H_
