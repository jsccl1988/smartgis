// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/touch_multitouch.h"

namespace ui {
namespace views {
namespace {

void clamp_multitouch_count(content::InputEvent* out) {
  if (out && out->pointer_count < 2) {
    out->pointer_count = 2;
  }
}

}  // namespace

bool TouchMultitouchTracker::midpoint(int32_t* x_px, int32_t* y_px) const {
  if (contacts_.size() < 2 || !x_px || !y_px) {
    return false;
  }
  int64_t sum_x = 0;
  int64_t sum_y = 0;
  for (const auto& entry : contacts_) {
    sum_x += entry.second.first;
    sum_y += entry.second.second;
  }
  const int64_t n = static_cast<int64_t>(contacts_.size());
  *x_px = static_cast<int32_t>(sum_x / n);
  *y_px = static_cast<int32_t>(sum_y / n);
  return true;
}

void TouchMultitouchTracker::fill_sample(content::InputEvent::Kind kind,
                                         content::InputEvent* out) const {
  if (!out) {
    return;
  }
  *out = content::InputEvent{};
  out->kind = kind;
  out->pointer_count = static_cast<uint32_t>(contacts_.size());
  int32_t x = 0;
  int32_t y = 0;
  if (midpoint(&x, &y)) {
    out->x_px = x;
    out->y_px = y;
  }
  clamp_multitouch_count(out);
}

bool TouchMultitouchTracker::on_contact_down(uint32_t id, int32_t x_px,
                                             int32_t y_px,
                                             content::InputEvent* out) {
  contacts_[id] = {x_px, y_px};
  if (contacts_.size() < 2) {
    return false;
  }
  if (!multitouch_active_) {
    multitouch_active_ = true;
    fill_sample(content::InputEvent::Kind::kLDown, out);
    return true;
  }
  fill_sample(content::InputEvent::Kind::kMouseMove, out);
  return true;
}

bool TouchMultitouchTracker::on_contact_move(uint32_t id, int32_t x_px,
                                             int32_t y_px,
                                             content::InputEvent* out) {
  auto it = contacts_.find(id);
  if (it == contacts_.end()) {
    return false;
  }
  it->second = {x_px, y_px};
  if (!multitouch_active_ || contacts_.size() < 2) {
    return false;
  }
  fill_sample(content::InputEvent::Kind::kMouseMove, out);
  return true;
}

bool TouchMultitouchTracker::on_contact_up(uint32_t id, int32_t x_px,
                                           int32_t y_px,
                                           content::InputEvent* out) {
  auto it = contacts_.find(id);
  if (it == contacts_.end()) {
    return false;
  }
  it->second = {x_px, y_px};
  const bool emit = multitouch_active_;
  content::InputEvent sample{};
  if (emit) {
    if (contacts_.size() >= 2) {
      fill_sample(content::InputEvent::Kind::kLUp, &sample);
    } else {
      sample.kind = content::InputEvent::Kind::kLUp;
      sample.x_px = x_px;
      sample.y_px = y_px;
      sample.pointer_count = 2;
    }
  }
  contacts_.erase(it);
  if (contacts_.size() < 2) {
    multitouch_active_ = false;
  }
  if (emit && out) {
    *out = sample;
    clamp_multitouch_count(out);
  }
  return emit;
}

void TouchMultitouchTracker::clear() {
  contacts_.clear();
  multitouch_active_ = false;
}

}  // namespace views
}  // namespace ui
