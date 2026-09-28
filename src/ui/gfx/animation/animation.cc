// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/animation/animation.h"

namespace ui {
namespace gfx {

Animation::~Animation() = default;

void Animation::start() {
  animating_ = true;
}

void Animation::stop() {
  animating_ = false;
}

void Animation::step(double progress) {
  if (progress < 0.0) {
    progress = 0.0;
  } else if (progress > 1.0) {
    progress = 1.0;
  }
  value_ = progress;
  on_progress(value_);
  if (value_ >= 1.0) {
    animating_ = false;
  }
}

void Animation::on_progress(double /*value*/) {}

}  // namespace gfx
}  // namespace ui
