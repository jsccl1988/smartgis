// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/install.h"


#include <cmath>
#include <string>
#include <tuple>

namespace content {
namespace code {
namespace {
bool apply_rc(content::CapabilityHost& host, int rc) {
  if (rc != 0) {
    host.fail_rc = rc;
    return false;
  }
  return true;
}

bool note_fail(content::CapabilityHost& host, bool ok, int fail_rc) {
  if (!ok && fail_rc != 0) {
    host.fail_rc = fail_rc;
  }
  return ok;
}

void detach_views(content::CapabilityHost& host) {
  if (host.view.detach_views) {
    host.view.detach_views();
  }
}

void stop_map_timers(content::CapabilityHost& host) {
  if (host.view.stop_present_timers) {
    host.view.stop_present_timers();
  }
}

void resume_map_timers(content::CapabilityHost& host) {
  if (host.view.resume_present_timers) {
    host.view.resume_present_timers();
  }
}

bool dispatch_edit(content::CapabilityHost& host,
                          const content::InputEvent& event) {
  return host.view.dispatch_edit_input && host.view.dispatch_edit_input(event);
}

bool release_exclusive(content::CapabilityHost& host) {
  return host.view.release_exclusive && host.view.release_exclusive();
}

bool run_tool(content::CapabilityHost& host,
                     std::string_view id,
                     int fail_rc) {
  if (id.empty() || !host.view.run_tool) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, host.view.run_tool(std::string(id)), fail_rc);
}

bool activate_tool(content::CapabilityHost& host, std::string_view id) {
  if (id.empty() || !host.view.activate_tool) {
    return false;
  }
  return apply_rc(host, host.view.activate_tool(std::string(id)));
}

std::string_view canonical_geom_kind(std::string_view kind) {
  if (kind == "line") {
    return "linestring";
  }
  if (kind == "poly") {
    return "polygon";
  }
  return kind;
}

bool read_last_geom(content::CapabilityHost& host,
                           content::GeomStatus* out) {
  if (!out || !host.view.last_geom || !host.view.last_geom(out)) {
    return false;
  }
  if (host.view.on_geom_event) {
    host.view.on_geom_event("view.geom", *out);
  }
  return true;
}

bool read_view_scale(content::CapabilityHost& host,
                            content::ViewScaleStatus* out) {
  if (!out || !host.view.view_scale || !host.view.view_scale(out)) {
    return false;
  }
  if (host.view.on_scale_event) {
    host.view.on_scale_event("view.scale", *out);
  }
  return true;
}

bool read_map_ready(content::CapabilityHost& host,
                           int timeout_ms,
                           content::MapReadyStatus* out) {
  if (!out || !host.view.map_ready_status ||
      !host.view.map_ready_status(timeout_ms, out)) {
    return false;
  }
  if (host.view.on_map_ready_event) {
    host.view.on_map_ready_event("map.ready", *out);
  }
  return true;
}

bool read_edit_tool_session(content::CapabilityHost& host,
                           content::EditToolSessionStatus* out) {
  if (!out || !host.view.edit_tool_session_status ||
      !host.view.edit_tool_session_status(out)) {
    return false;
  }
  if (host.view.on_edit_tool_session_event) {
    host.view.on_edit_tool_session_event("view.edit_tool_session", *out);
  }
  return true;
}

bool read_tool_status(content::CapabilityHost& host,
                             content::ToolStatus* out) {
  if (!out || !host.view.tool_status || !host.view.tool_status(out)) {
    return false;
  }
  if (host.view.on_tool_event) {
    host.view.on_tool_event("view.tool", *out);
  }
  return true;
}

bool read_view_load(content::CapabilityHost& host,
                           std::string_view face,
                           int timeout_ms,
                           content::ViewLoadStatus* out) {
  if (!out || !host.view.load_status ||
      !host.view.load_status(std::string(face), timeout_ms, out)) {
    return false;
  }
  if (host.view.on_app_event) {
    host.view.on_app_event("map.load", *out);
  }
  return true;
}

bool wait_map_ready(content::CapabilityHost& host, int timeout_ms) {
  content::MapReadyStatus status;
  if (!read_map_ready(host, timeout_ms, &status)) {
    return false;
  }
  return status.layout_built != 0;
}

bool edit_tool_session_ready(content::CapabilityHost& host) {
  content::EditToolSessionStatus status;
  if (!read_edit_tool_session(host, &status)) {
    return false;
  }
  return status.memory_session != 0;
}

bool expect_tool(content::CapabilityHost& host,
                        std::string_view id,
                        int fail_rc) {
  if (id.empty()) {
    return note_fail(host, false, fail_rc);
  }
  content::ToolStatus status;
  if (!read_tool_status(host, &status)) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, status.id == id, fail_rc);
}

bool expect_geom(content::CapabilityHost& host,
                        std::string_view kind,
                        int min_points) {
  content::GeomStatus got;
  if (!read_last_geom(host, &got)) {
    return false;
  }
  const std::string_view want = canonical_geom_kind(kind);
  const int need = min_points > 0 ? min_points : 1;
  return got.is_append != 0 && !got.kind.empty() && got.kind == want &&
         got.point_count >= need;
}

bool expect_wheel_cursor(content::CapabilityHost& host, int x, int y) {
  content::ViewScaleStatus before;
  if (!read_view_scale(host, &before)) {
    return false;
  }
  if (!before.has_frame) {
    if (host.horizon.mark) {
      host.horizon.mark("wheel-cursor-no-frame");
    }
    return false;
  }
  const auto send = [&](int delta) {
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = x;
    wheel.y_px = y;
    wheel.wheel = delta;
    return dispatch_edit(host, wheel);
  };
  if (!send(-120)) {
    return false;
  }
  content::ViewScaleStatus after;
  if (!read_view_scale(host, &after)) {
    return false;
  }
  if (std::fabs(after.scale - before.scale) < 1e-9) {
    if (!send(120) || !read_view_scale(host, &after) ||
        std::fabs(after.scale - before.scale) < 1e-9) {
      return false;
    }
  }
  return after.ortho != 0;
}


bool eval_stop_map_timers(content::CapabilityHost& host,
                          const Module&,
                          const Inst&,
                          SlotTable&,
                          std::string*) {
  stop_map_timers(host);
  return true;
}

bool eval_resume_map_timers(content::CapabilityHost& host,
                            const Module&,
                            const Inst&,
                            SlotTable&,
                            std::string*) {
  resume_map_timers(host);
  return true;
}

bool eval_run_tool(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  return ok_step(inst,
                 run_tool(host, arg_str_param(module, inst, slots, 0),
                              arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_activate_tool(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  return ok_step(inst, activate_tool(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_wait_map_ready(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  const bool ready =
      wait_map_ready(host, arg_i32_param(module, inst, slots, 0));
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
  return ok_step(inst, edit_tool_session_ready(host), error);
}

bool eval_expect_tool(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst,
                 expect_tool(host, arg_str_param(module, inst, slots, 0),
                                 arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_expect_geom(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst,
                 expect_geom(host, arg_str_param(module, inst, slots, 0),
                                 arg_i32_param(module, inst, slots, 1)),
                 error);
}

bool eval_expect_wheel_cursor(content::CapabilityHost& host,
                              const Module& module,
                              const Inst& inst,
                              SlotTable& slots,
                              std::string* error) {
  return ok_step(inst,
                 expect_wheel_cursor(host, arg_i32_param(module, inst, slots, 0),
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

CONTENT_LANE_OP(StopMapTimersOp, Op::kStopMapTimers, "stop_map_timers", nullptr,
                nullptr, 0, false, eval_stop_map_timers);
CONTENT_LANE_OP(ResumeMapTimersOp, Op::kResumeMapTimers, "resume_map_timers", nullptr,
                nullptr, 0, false, eval_resume_map_timers);
CONTENT_LANE_OP(RunToolOp, Op::kRunTool, "tool", kRunToolAliases, kIdFailParams, 2,
                false, eval_run_tool);
CONTENT_LANE_OP(ActivateToolOp, Op::kActivateTool, "activate_tool", nullptr, kIdParams,
                1, false, eval_activate_tool);
CONTENT_LANE_OP(WaitMapReadyOp, Op::kWaitMapReady, "wait_ready", kWaitAliases,
                kWaitReadyParams, 2, false, eval_wait_map_ready);
CONTENT_LANE_OP(EditToolSessionReadyOp, Op::kEditToolSessionReady,
                "require_edit_tool_session", kHostAliases, nullptr, 0, false,
                eval_edit_tool_session_ready);
CONTENT_LANE_OP(ExpectToolOp, Op::kExpectTool, "expect_tool", nullptr, kIdFailParams, 2,
                false, eval_expect_tool);
CONTENT_LANE_OP(ExpectGeomOp, Op::kExpectGeom, "expect_geom", nullptr, kGeomParams, 2,
                false, eval_expect_geom);
CONTENT_LANE_OP(ExpectWheelCursorOp, Op::kExpectWheelCursor, "expect_wheel_cursor",
                nullptr, kWheelCursorParams, 2, false, eval_expect_wheel_cursor);

using ViewOps = std::tuple<StopMapTimersOp,
                           ResumeMapTimersOp,
                           RunToolOp,
                           ActivateToolOp,
                           WaitMapReadyOp,
                           EditToolSessionReadyOp,
                           ExpectToolOp,
                           ExpectGeomOp,
                           ExpectWheelCursorOp>;

}  // namespace

void fill_view_ops(const OpDesc** by_op) {
  detail::install_ops<ViewOps>(by_op);
}

}  // namespace code
}  // namespace content
