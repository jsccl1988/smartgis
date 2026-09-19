// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_TOUCH_MULTITOUCH_H_
#define UI_VIEWS_MAP_TOUCH_MULTITOUCH_H_

#include <cstdint>
#include <unordered_map>
#include <utility>

#include "content/public/map_types.h"

namespace ui {
namespace views {

// Tracks PT_TOUCH contacts and builds content::InputEvent samples for
// two-or-more-finger midpoint pan (same contract as CEF shell.js).
// Single-finger stays out of this path so mouse synthesis keeps working.
class TouchMultitouchTracker {
 public:
  // Fills |out| when a multitouch InputEvent should be dispatched.
  bool on_contact_down(uint32_t id, int32_t x_px, int32_t y_px,
                       content::InputEvent* out);
  bool on_contact_move(uint32_t id, int32_t x_px, int32_t y_px,
                       content::InputEvent* out);
  bool on_contact_up(uint32_t id, int32_t x_px, int32_t y_px,
                     content::InputEvent* out);

  // While true, HWND mouse messages must be ignored (primary-contact
  // synthesis would otherwise fight the midpoint stream).
  bool suppress_mouse() const { return multitouch_active_; }

  void clear();
  size_t contact_count() const { return contacts_.size(); }

 private:
  bool midpoint(int32_t* x_px, int32_t* y_px) const;
  void fill_sample(content::InputEvent::Kind kind, content::InputEvent* out) const;

  std::unordered_map<uint32_t, std::pair<int32_t, int32_t>> contacts_;
  bool multitouch_active_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_TOUCH_MULTITOUCH_H_
