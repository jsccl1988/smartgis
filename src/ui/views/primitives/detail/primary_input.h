// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_DETAIL_PRIMARY_INPUT_H_
#define UI_VIEWS_PRIMITIVES_DETAIL_PRIMARY_INPUT_H_

#include "ui/views/kernel/shell/event.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace detail {

inline bool is_primary_down(const MouseEvent& e) {
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

inline bool is_primary_up(const MouseEvent& e) {
  return e.type == MouseEvent::Type::kUp && e.button == 1;
}

// Press is consumed (so Widget capture sticks). Release runs |activate|.
// Matches the existing Button/Checkbox/Radio contract, including tests that
// send mouse-up without a matching down.
template <typename Fn>
bool handle_primary_click(const MouseEvent& e, bool enabled, Fn&& activate) {
  if (!enabled) {
    return false;
  }
  if (is_primary_up(e)) {
    activate();
    return true;
  }
  return is_primary_down(e);
}

template <typename Fn>
bool handle_activate_key(const KeyEvent& e, bool enabled, bool allow_return,
                         Fn&& activate) {
  if (!enabled || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_SPACE || (allow_return && e.vk == VK_RETURN)) {
    activate();
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_DETAIL_PRIMARY_INPUT_H_
