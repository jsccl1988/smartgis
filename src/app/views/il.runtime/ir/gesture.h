// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_GESTURE_H_
#define IL_RUNTIME_IR_GESTURE_H_

#include <string_view>
#include <vector>

#include "app/views/il.runtime/ir/horizon.h"
#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Pointer gestures. map_client goes through the edit host; anything else is
// an HWND inject. Languages decode coordinates, then call these.

inline bool is_map_target(std::string_view target) {
  return target == "map_client";
}

inline bool dispatch_edit(content::CapabilityHost& host,
                          const content::InputEvent& event) {
  return host.dispatch_edit_input && host.dispatch_edit_input(event);
}

inline content::InputEvent pointer_event(content::InputEvent::Kind kind,
                                         int x,
                                         int y,
                                         int wheel = 0) {
  content::InputEvent event{};
  event.kind = kind;
  event.x_px = x;
  event.y_px = y;
  event.wheel = wheel;
  return event;
}

inline bool post_click(content::CapabilityHost& host,
                       int x,
                       int y,
                       int button,
                       int clicks) {
  return host.post_click && host.post_click(x, y, button, clicks);
}

inline bool post_drag(content::CapabilityHost& host,
                      int x0,
                      int y0,
                      int x1,
                      int y1) {
  return host.post_drag && host.post_drag(x0, y0, x1, y1);
}

inline bool post_wheel(content::CapabilityHost& host, int x, int y, int delta) {
  return host.post_wheel && host.post_wheel(x, y, delta);
}

inline bool post_path(content::CapabilityHost& host,
                      const std::vector<int>& xs,
                      const std::vector<int>& ys) {
  return xs.size() >= 2 && xs.size() == ys.size() && host.post_path &&
         host.post_path(xs, ys);
}

inline bool drag_edit(content::CapabilityHost& host,
                      int x0,
                      int y0,
                      int x1,
                      int y1) {
  using Kind = content::InputEvent::Kind;
  return dispatch_edit(host, pointer_event(Kind::kLDown, x0, y0)) &&
         dispatch_edit(host, pointer_event(Kind::kMouseMove, x1, y1)) &&
         dispatch_edit(host, pointer_event(Kind::kLUp, x1, y1));
}

inline bool click(content::CapabilityHost& host,
                  std::string_view target,
                  int x,
                  int y,
                  int button,
                  int clicks) {
  using Kind = content::InputEvent::Kind;
  const Kind down = button != 0 ? Kind::kRDown : Kind::kLDown;
  if (is_map_target(target) &&
      dispatch_edit(host, pointer_event(down, x, y))) {
    if (clicks >= 2) {
      dispatch_edit(host, pointer_event(down, x, y));
    }
    return true;
  }
  return post_click(host, x, y, button, clicks);
}

inline bool drag(content::CapabilityHost& host,
                 std::string_view target,
                 int x0,
                 int y0,
                 int x1,
                 int y1) {
  if (is_map_target(target) && drag_edit(host, x0, y0, x1, y1)) {
    return true;
  }
  return post_drag(host, x0, y0, x1, y1);
}

inline bool wheel(content::CapabilityHost& host,
                  std::string_view target,
                  int x,
                  int y,
                  int delta) {
  if (is_map_target(target) &&
      dispatch_edit(host, pointer_event(content::InputEvent::Kind::kWheel, x, y,
                                        delta))) {
    return true;
  }
  return post_wheel(host, x, y, delta);
}

inline bool stroke(content::CapabilityHost& host,
                   std::string_view target,
                   const std::vector<int>& xs,
                   const std::vector<int>& ys) {
  if (xs.size() < 2 || xs.size() != ys.size()) {
    return false;
  }
  if (is_map_target(target)) {
    using Kind = content::InputEvent::Kind;
    if (!dispatch_edit(host, pointer_event(Kind::kLDown, xs[0], ys[0]))) {
      return post_path(host, xs, ys);
    }
    for (size_t i = 1; i < xs.size(); ++i) {
      if (!dispatch_edit(host, pointer_event(Kind::kMouseMove, xs[i], ys[i]))) {
        return false;
      }
    }
    return dispatch_edit(host, pointer_event(Kind::kLUp, xs.back(), ys.back()));
  }
  return post_path(host, xs, ys);
}

inline bool pan_burst(content::CapabilityHost& host,
                      std::string_view target,
                      int count,
                      int x,
                      int y,
                      int dx,
                      int dy,
                      int pump_ms) {
  const bool use_map = is_map_target(target);
  for (int i = 0; i < count; ++i) {
    const int x0 = x + (i % 5) * 8;
    const int y0 = y + (i % 7) * 6;
    const int x1 = x0 + dx;
    const int y1 = y0 + dy;
    if (use_map) {
      if (!drag_edit(host, x0, y0, x1, y1) &&
          !post_drag(host, x0, y0, x1, y1)) {
        return false;
      }
    } else if (!post_drag(host, x0, y0, x1, y1)) {
      return false;
    }
    if (pump_ms > 0) {
      pump(host, pump_ms);
    }
  }
  return true;
}

inline bool wheel_burst(content::CapabilityHost& host,
                        std::string_view target,
                        int count,
                        int x,
                        int y,
                        int delta,
                        int pump_ms) {
  const bool use_map = is_map_target(target);
  for (int i = 0; i < count; ++i) {
    const int step = (i % 2 == 0) ? delta : -delta;
    if (use_map) {
      if (!dispatch_edit(host, pointer_event(content::InputEvent::Kind::kWheel,
                                             x, y, step)) &&
          !post_wheel(host, x, y, step)) {
        return false;
      }
    } else if (!post_wheel(host, x, y, step)) {
      return false;
    }
    if (pump_ms > 0) {
      pump(host, pump_ms);
    }
  }
  return true;
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_GESTURE_H_
