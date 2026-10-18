// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "content/browser/glang.runtime/language/ir/gesture.h"

namespace content {
namespace code {
namespace {

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
                 ir::click(host, arg_str_param(module, inst, slots, 0),
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
                 ir::drag(host, arg_str_param(module, inst, slots, 0), x0, y0, x1, y1),
                 error);
}

bool eval_wheel(content::CapabilityHost& host,
                const Module& module,
                const Inst& inst,
                SlotTable& slots,
                std::string* error) {
  return ok_step(inst,
                 ir::wheel(host, arg_str_param(module, inst, slots, 0),
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
  return ok_step(inst, ir::stroke(host, target, xs, ys), error);
}

bool eval_pan_burst(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
  return ok_step(inst,
                 ir::pan_burst(host, arg_str_param(module, inst, slots, 0),
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
                 ir::wheel_burst(host, arg_str_param(module, inst, slots, 0),
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

const OpDesc kClick{
    Op::kClick, "click", nullptr, kClickParams, 5, false, &eval_click,
};
const OpDesc kDrag{Op::kDrag, "drag", nullptr, kDragParams, 5, false, &eval_drag};
const OpDesc kWheel{
    Op::kWheel, "wheel", nullptr, kWheelParams, 4, false, &eval_wheel,
};
const OpDesc kStroke{
    Op::kStroke, "stroke", nullptr, kStrokeParams, 1, false, &eval_stroke,
};
const OpDesc kPanBurst{
    Op::kPanBurst, "pan_burst", nullptr, kPanParams, 7, false, &eval_pan_burst,
};
const OpDesc kWheelBurst{
    Op::kWheelBurst, "wheel_burst", nullptr, kWheelBurstParams, 6, false,
    &eval_wheel_burst,
};

}  // namespace

void fill_gesture_ops(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  by_op[static_cast<std::size_t>(Op::kClick)] = &kClick;
  by_op[static_cast<std::size_t>(Op::kDrag)] = &kDrag;
  by_op[static_cast<std::size_t>(Op::kWheel)] = &kWheel;
  by_op[static_cast<std::size_t>(Op::kStroke)] = &kStroke;
  by_op[static_cast<std::size_t>(Op::kPanBurst)] = &kPanBurst;
  by_op[static_cast<std::size_t>(Op::kWheelBurst)] = &kWheelBurst;
}

}  // namespace code
}  // namespace content
