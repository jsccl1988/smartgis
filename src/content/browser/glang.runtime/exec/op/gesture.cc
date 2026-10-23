// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/install.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>


namespace content {
namespace code {
namespace {
bool dispatch_edit(content::CapabilityHost& host,
                          const content::InputEvent& event) {
  return host.view.dispatch_edit_input && host.view.dispatch_edit_input(event);
}

void pump(content::CapabilityHost& host, int ms) {
  if (host.horizon.pump) {
    host.horizon.pump(ms);
  }
}

bool post_click(content::CapabilityHost& host,
                       int x,
                       int y,
                       int button,
                       int clicks) {
  return host.horizon.post_click &&
         host.horizon.post_click(x, y, button, clicks);
}

bool post_drag(content::CapabilityHost& host,
                      int x0,
                      int y0,
                      int x1,
                      int y1) {
  return host.horizon.post_drag && host.horizon.post_drag(x0, y0, x1, y1);
}

bool post_wheel(content::CapabilityHost& host, int x, int y, int delta) {
  return host.horizon.post_wheel && host.horizon.post_wheel(x, y, delta);
}

bool post_path(content::CapabilityHost& host,
                      const std::vector<int>& xs,
                      const std::vector<int>& ys) {
  return xs.size() >= 2 && xs.size() == ys.size() && host.horizon.post_path &&
         host.horizon.post_path(xs, ys);
}

bool is_map_target(std::string_view target) {
  return target == "map_client";
}

content::InputEvent pointer_event(content::InputEvent::Kind kind,
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

bool drag_edit(content::CapabilityHost& host,
                      int x0,
                      int y0,
                      int x1,
                      int y1) {
  using Kind = content::InputEvent::Kind;
  return dispatch_edit(host, pointer_event(Kind::kLDown, x0, y0)) &&
         dispatch_edit(host, pointer_event(Kind::kMouseMove, x1, y1)) &&
         dispatch_edit(host, pointer_event(Kind::kLUp, x1, y1));
}

bool click(content::CapabilityHost& host,
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

bool drag(content::CapabilityHost& host,
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

bool wheel(content::CapabilityHost& host,
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

bool stroke(content::CapabilityHost& host,
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

bool pan_burst(content::CapabilityHost& host,
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

bool wheel_burst(content::CapabilityHost& host,
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


int click_button(const Module& module, const Inst& inst, const SlotTable& slots) {
  const Operand* operand = nth_operand(module, inst, 3);
  if (!operand) {
    return 0;
  }
  if (operand->kind == ArgKind::kI32 || operand->kind == ArgKind::kBool) {
    return operand->i32;
  }
  const std::string button = expand_operand(module, *operand, slots);
  if (button == "right") {
    return 1;
  }
  if (button.empty() || button == "left") {
    return 0;
  }
  return std::atoi(button.c_str());
}

bool eval_click(content::CapabilityHost& host,
                const Module& module,
                const Inst& inst,
                SlotTable& slots,
                std::string* error) {
  return ok_step(inst,
                 click(host, arg_str_param(module, inst, slots, 0),
                           arg_i32_param(module, inst, slots, 1),
                           arg_i32_param(module, inst, slots, 2),
                           click_button(module, inst, slots),
                           arg_i32_param(module, inst, slots, 4)),
                 error);
}

bool eval_drag(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string* error) {
  const int x0 = arg_i32_param(module, inst, slots, 1);
  const int y0 = arg_i32_param(module, inst, slots, 2);
  // Omitted x1/y1 repeat the press point, matching the script interpreter.
  const int x1 = arg_i32(module, inst, slots, 3, x0);
  const int y1 = arg_i32(module, inst, slots, 4, y0);
  return ok_step(inst,
                 drag(host, arg_str_param(module, inst, slots, 0), x0, y0, x1, y1),
                 error);
}

bool eval_wheel(content::CapabilityHost& host,
                const Module& module,
                const Inst& inst,
                SlotTable& slots,
                std::string* error) {
  return ok_step(inst,
                 wheel(host, arg_str_param(module, inst, slots, 0),
                           arg_i32_param(module, inst, slots, 1),
                           arg_i32_param(module, inst, slots, 2),
                           arg_i32_param(module, inst, slots, 3)),
                 error);
}

// Target is operand 0 when it is text. Following operands are x,y integer pairs
// (sema already split "x,y"). A leading i32 means the target was omitted.
bool eval_stroke(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  std::string target = "shell_client";
  int begin = 0;
  if (const Operand* operand = nth_operand(module, inst, 0)) {
    if (operand->kind == ArgKind::kI32 || operand->kind == ArgKind::kBool) {
      begin = 0;
    } else {
      const std::string text = expand_operand(module, *operand, slots);
      if (!text.empty()) {
        target = text;
      }
      begin = 1;
    }
  }
  std::vector<int> xs;
  std::vector<int> ys;
  for (int i = begin; i + 1 < static_cast<int>(inst.operand_count); i += 2) {
    xs.push_back(arg_i32(module, inst, slots, i, 0));
    ys.push_back(arg_i32(module, inst, slots, i + 1, 0));
  }
  return ok_step(inst, stroke(host, target, xs, ys), error);
}

bool eval_pan_burst(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
  return ok_step(inst,
                 pan_burst(host, arg_str_param(module, inst, slots, 0),
                               arg_i32_param(module, inst, slots, 1),
                               arg_i32_param(module, inst, slots, 2),
                               arg_i32_param(module, inst, slots, 3),
                               arg_i32_param(module, inst, slots, 4),
                               arg_i32_param(module, inst, slots, 5),
                               arg_i32_param(module, inst, slots, 6)),
                 error);
}

bool eval_wheel_burst(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst,
                 wheel_burst(host, arg_str_param(module, inst, slots, 0),
                                 arg_i32_param(module, inst, slots, 1),
                                 arg_i32_param(module, inst, slots, 2),
                                 arg_i32_param(module, inst, slots, 3),
                                 arg_i32_param(module, inst, slots, 4),
                                 arg_i32_param(module, inst, slots, 5)),
                 error);
}

constexpr Param kClickParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "shell_client"},
    {"x", 1, ArgKind::kI32, false, 0, nullptr},
    {"y", 2, ArgKind::kI32, false, 0, nullptr},
    {"button", 3, ArgKind::kStr, false, 0, "left"},
    {"clicks", 4, ArgKind::kI32, false, 1, nullptr},
};
constexpr Param kDragParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "shell_client"},
    {"x0", 1, ArgKind::kI32, false, 0, nullptr},
    {"y0", 2, ArgKind::kI32, false, 0, nullptr},
    {"x1", 3, ArgKind::kI32, false, 0, nullptr},
    {"y1", 4, ArgKind::kI32, false, 0, nullptr},
};
constexpr Param kWheelParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "shell_client"},
    {"x", 1, ArgKind::kI32, false, 0, nullptr},
    {"y", 2, ArgKind::kI32, false, 0, nullptr},
    {"delta", 3, ArgKind::kI32, false, -120, nullptr},
};
constexpr Param kStrokeParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "shell_client"},
};
constexpr Param kPanParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "map_client"},
    {"count", -1, ArgKind::kI32, false, 4, nullptr},
    {"x", -1, ArgKind::kI32, false, 200, nullptr},
    {"y", -1, ArgKind::kI32, false, 300, nullptr},
    {"dx", -1, ArgKind::kI32, false, 40, nullptr},
    {"dy", -1, ArgKind::kI32, false, 24, nullptr},
    {"pump_ms", -1, ArgKind::kI32, false, 40, nullptr},
};
constexpr Param kWheelBurstParams[] = {
    {"target", 0, ArgKind::kStr, false, 0, "map_client"},
    {"count", -1, ArgKind::kI32, false, 3, nullptr},
    {"x", -1, ArgKind::kI32, false, 400, nullptr},
    {"y", -1, ArgKind::kI32, false, 400, nullptr},
    {"delta", -1, ArgKind::kI32, false, -120, nullptr},
    {"pump_ms", -1, ArgKind::kI32, false, 50, nullptr},
};

CONTENT_LANE_OP(ClickOp, Op::kClick, "click", nullptr, kClickParams, 5, false,
                eval_click);
CONTENT_LANE_OP(DragOp, Op::kDrag, "drag", nullptr, kDragParams, 5, false, eval_drag);
CONTENT_LANE_OP(WheelOp, Op::kWheel, "wheel", nullptr, kWheelParams, 4, false,
                eval_wheel);
CONTENT_LANE_OP(StrokeOp, Op::kStroke, "stroke", nullptr, kStrokeParams, 1, false,
                eval_stroke);
CONTENT_LANE_OP(PanBurstOp, Op::kPanBurst, "pan_burst", nullptr, kPanParams, 7, false,
                eval_pan_burst);
CONTENT_LANE_OP(WheelBurstOp, Op::kWheelBurst, "wheel_burst", nullptr, kWheelBurstParams,
                6, false, eval_wheel_burst);

using GestureOps =
    std::tuple<ClickOp, DragOp, WheelOp, StrokeOp, PanBurstOp, WheelBurstOp>;

}  // namespace

void fill_gesture_ops(const OpDesc** by_op) {
  detail::install_ops<GestureOps>(by_op);
}

}  // namespace code
}  // namespace content
