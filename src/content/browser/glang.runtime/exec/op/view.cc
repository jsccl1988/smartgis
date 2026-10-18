// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"

#include "content/browser/glang.runtime/language/ir/view.h"

namespace content {
namespace code {
namespace {

bool eval_stop_map_timers(content::CapabilityHost& host,
                          const Module&,
                          const Inst&,
                          SlotTable&,
                          std::string*) {
  ir::stop_map_timers(host);
  return true;
}

bool eval_resume_map_timers(content::CapabilityHost& host,
                            const Module&,
                            const Inst&,
                            SlotTable&,
                            std::string*) {
  ir::resume_map_timers(host);
  return true;
}

bool eval_run_tool(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  return ok_step(inst,
                 ir::run_tool(host, arg_str_param(module, inst, slots, 0),
                              arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_activate_tool(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  return ok_step(inst, ir::activate_tool(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_wait_map_ready(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  const bool ready =
      ir::wait_map_ready(host, arg_i32_param(module, inst, slots, 0));
  if (arg_bool_param(module, inst, slots, 1)) {
    return true;
  }
  return ok_step(inst, ready, error);
}

bool eval_edit_tool_session_ready(content::CapabilityHost& host,
                                  const Module&,
                                  const Inst& inst,
                                  SlotTable&,
                                  std::string* error) {
  return ok_step(inst, ir::edit_tool_session_ready(host), error);
}

bool eval_expect_tool(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst,
                 ir::expect_tool(host, arg_str_param(module, inst, slots, 0),
                                 arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_expect_geom(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst,
                 ir::expect_geom(host, arg_str_param(module, inst, slots, 0),
                                 arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_expect_wheel_cursor(content::CapabilityHost& host,
                              const Module& module,
                              const Inst& inst,
                              SlotTable& slots,
                              std::string* error) {
  return ok_step(inst,
                 ir::expect_wheel_cursor(host, arg_i32_param(module, inst, slots, 0),
                                         arg_i32_param(module, inst, slots, 1)),
                 error);
}

constexpr Param kIdFailParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
    {"fail_rc", -1, ArgKind::kI32, false, 0, nullptr},
};
constexpr Param kIdParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kWaitReadyParams[] = {
    {"ms", 0, ArgKind::kI32, false, 5000, nullptr},
    {"soft", -1, ArgKind::kBool, false, 0, nullptr},
};
constexpr Param kGeomParams[] = {
    {"kind", 0, ArgKind::kStr, false, 0, "point"},
    {"min_points", 1, ArgKind::kI32, false, 1, nullptr},
};
constexpr Param kWheelCursorParams[] = {
    {"x", 0, ArgKind::kI32, false, 40, nullptr},
    {"y", 1, ArgKind::kI32, false, 40, nullptr},
};

constexpr const char* const kRunToolAliases[] = {"run_tool", nullptr};
constexpr const char* const kWaitAliases[] = {"wait_map_ready", nullptr};
constexpr const char* const kHostAliases[] = {"expect_host", nullptr};

const OpDesc kStopMapTimers{
    Op::kStopMapTimers, "stop_map_timers", nullptr, nullptr, 0, false,
    &eval_stop_map_timers,
};
const OpDesc kResumeMapTimers{
    Op::kResumeMapTimers, "resume_map_timers", nullptr, nullptr, 0, false,
    &eval_resume_map_timers,
};
const OpDesc kRunTool{
    Op::kRunTool, "tool", kRunToolAliases, kIdFailParams, 2, false, &eval_run_tool,
};
const OpDesc kActivateTool{
    Op::kActivateTool, "activate_tool", nullptr, kIdParams, 1, false,
    &eval_activate_tool,
};
const OpDesc kWaitMapReady{
    Op::kWaitMapReady, "wait_ready", kWaitAliases, kWaitReadyParams, 2, false,
    &eval_wait_map_ready,
};
const OpDesc kEditToolSessionReady{
    Op::kEditToolSessionReady, "require_edit_tool_session", kHostAliases, nullptr, 0,
    false, &eval_edit_tool_session_ready,
};
const OpDesc kExpectTool{
    Op::kExpectTool, "expect_tool", nullptr, kIdFailParams, 2, false, &eval_expect_tool,
};
const OpDesc kExpectGeom{
    Op::kExpectGeom, "expect_geom", nullptr, kGeomParams, 2, false, &eval_expect_geom,
};
const OpDesc kExpectWheelCursor{
    Op::kExpectWheelCursor, "expect_wheel_cursor", nullptr, kWheelCursorParams, 2,
    false, &eval_expect_wheel_cursor,
};

}  // namespace

void fill_view_ops(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  by_op[static_cast<std::size_t>(Op::kStopMapTimers)] = &kStopMapTimers;
  by_op[static_cast<std::size_t>(Op::kResumeMapTimers)] = &kResumeMapTimers;
  by_op[static_cast<std::size_t>(Op::kRunTool)] = &kRunTool;
  by_op[static_cast<std::size_t>(Op::kActivateTool)] = &kActivateTool;
  by_op[static_cast<std::size_t>(Op::kWaitMapReady)] = &kWaitMapReady;
  by_op[static_cast<std::size_t>(Op::kEditToolSessionReady)] = &kEditToolSessionReady;
  by_op[static_cast<std::size_t>(Op::kExpectTool)] = &kExpectTool;
  by_op[static_cast<std::size_t>(Op::kExpectGeom)] = &kExpectGeom;
  by_op[static_cast<std::size_t>(Op::kExpectWheelCursor)] = &kExpectWheelCursor;
}

}  // namespace code
}  // namespace content
