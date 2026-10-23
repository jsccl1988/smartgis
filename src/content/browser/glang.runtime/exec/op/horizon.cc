// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/install.h"

#include <cstdlib>
#include <string>
#include <tuple>
#include <vector>


namespace content {
namespace code {
namespace {
void detach_views(content::CapabilityHost& host) {
  if (host.view.detach_views) {
    host.view.detach_views();
  }
}

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

void pump(content::CapabilityHost& host, int ms) {
  if (host.horizon.pump) {
    host.horizon.pump(ms);
  }
}

void mark(content::CapabilityHost& host, std::string_view token) {
  if (host.horizon.mark) {
    host.horizon.mark(std::string(token));
  }
}

void clear_marks(content::CapabilityHost& host) {
  if (host.horizon.clear_marks) {
    host.horizon.clear_marks();
  }
}

void select_view_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.select_view_tab) {
    host.horizon.select_view_tab(index);
  }
}

void catalog_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.catalog_tab) {
    host.horizon.catalog_tab(index);
  }
}

void inspector_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.inspector_tab) {
    host.horizon.inspector_tab(index);
  }
}

void* shell_hwnd(content::CapabilityHost& host) {
  return host.horizon.shell_hwnd ? host.horizon.shell_hwnd() : nullptr;
}

bool read_shell_status(content::CapabilityHost& host,
                              content::ShellStatus* out) {
  if (!out || !host.horizon.shell_status || !host.horizon.shell_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.shell = *out;
    host.horizon.on_app_event("shell.tree", fact);
  }
  return true;
}

bool read_layout_status(content::CapabilityHost& host,
                               content::LayoutStatus* out) {
  if (!out || !host.horizon.layout_status ||
      !host.horizon.layout_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.layout = *out;
    host.horizon.on_app_event("shell.layout", fact);
  }
  return true;
}

bool read_hwnd_status(content::CapabilityHost& host,
                             content::HwndStatus* out) {
  if (!out || !host.horizon.hwnd_status || !host.horizon.hwnd_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.hwnd = *out;
    host.horizon.on_app_event("shell.hwnd", fact);
  }
  return true;
}

bool require_hwnd(content::CapabilityHost& host) {
  content::HwndStatus status;
  if (!read_hwnd_status(host, &status)) {
    return false;
  }
  if (status.alive) {
    return true;
  }
  if (host.view.detach_views) {
    host.view.detach_views();
  }
  host.fail_rc = 2;
  return false;
}

bool window(content::CapabilityHost& host,
                   std::string_view action,
                   int w,
                   int h) {
  return host.horizon.window && host.horizon.window(std::string(action), w, h);
}

unsigned vk_from_name(content::CapabilityHost& host,
                             std::string_view name) {
  if (!host.horizon.vk_from_name || name.empty()) {
    return 0;
  }
  return host.horizon.vk_from_name(std::string(name));
}

bool key(content::CapabilityHost& host, unsigned vk) {
  return host.horizon.key && host.horizon.key(vk);
}

bool post_key(content::CapabilityHost& host, unsigned vk, bool down) {
  return host.horizon.post_key && host.horizon.post_key(vk, down);
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

bool suppress_dialogs(content::CapabilityHost& host, bool on) {
  return host.horizon.suppress_dialogs && host.horizon.suppress_dialogs(on);
}

bool expect_shell_tree(content::CapabilityHost& host) {
  content::ShellStatus status;
  if (!read_shell_status(host, &status)) {
    return false;
  }
  if (!status.root_present || status.child_count < 3) {
    return apply_rc(host, 4);
  }
  if (!status.columns_present || status.column_count < 2) {
    return apply_rc(host, 5);
  }
  if (!status.catalog_present || !status.layer_tree) {
    return apply_rc(host, 6);
  }
  return true;
}

bool expect_layout_bounds(content::CapabilityHost& host) {
  content::LayoutStatus status;
  if (!read_layout_status(host, &status)) {
    return false;
  }
  if (status.violation_count > 0) {
    mark(host, "layout-fail");
    if (host.view.detach_views) {
      host.view.detach_views();
    }
    return apply_rc(host, 30);
  }
  return true;
}

bool wire_debug_agent(content::CapabilityHost& host) {
  return host.horizon.wire_debug_agent &&
         apply_rc(host, host.horizon.wire_debug_agent());
}

bool debug_exec(content::CapabilityHost& host,
                       std::string_view line,
                       std::string_view contains,
                       std::string_view equals,
                       std::string_view reject,
                       int fail_rc) {
  if (line.empty() || !host.horizon.debug_exec) {
    return false;
  }
  return apply_rc(host, host.horizon.debug_exec(
                            std::string(line), std::string(contains),
                            std::string(equals), std::string(reject), fail_rc));
}

bool console_pan_bench(content::CapabilityHost& host) {
  return host.horizon.console_pan_bench &&
         apply_rc(host, host.horizon.console_pan_bench());
}

bool apply_ui_theme(content::CapabilityHost& host) {
  return host.horizon.apply_ui_theme && host.horizon.apply_ui_theme();
}

bool apply_scenario_panels(content::CapabilityHost& host,
                                  std::string_view mode) {
  return host.horizon.apply_scenario_panels &&
         host.horizon.apply_scenario_panels(std::string(mode));
}

bool layout_gate(content::CapabilityHost& host, std::string_view mode) {
  if (!host.horizon.layout_gate) {
    return false;
  }
  return apply_rc(host, host.horizon.layout_gate(std::string(mode)));
}

bool ui_present_capture(content::CapabilityHost& host,
                               std::string_view mode) {
  if (!host.horizon.ui_present_capture) {
    return false;
  }
  return apply_rc(host, host.horizon.ui_present_capture(std::string(mode)));
}


unsigned vk_of(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               const SlotTable& slots,
               int index) {
  const Operand* operand = nth_operand(module, inst, index);
  if (!operand) {
    return 0;
  }
  if (operand->kind == ArgKind::kI32 || operand->kind == ArgKind::kBool) {
    return static_cast<unsigned>(operand->i32);
  }
  const std::string name = expand_operand(module, *operand, slots);
  unsigned vk = vk_from_name(host, name);
  if (vk == 0) {
    vk = static_cast<unsigned>(std::atoi(name.c_str()));
  }
  return vk;
}

bool eval_detach_views(content::CapabilityHost& host,
                       const Module&,
                       const Inst&,
                       SlotTable&,
                       std::string*) {
  detach_views(host);
  return true;
}

bool eval_pump(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string*) {
  pump(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_mark(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string*) {
  mark(host, arg_str_param(module, inst, slots, 0));
  return true;
}

bool eval_clear_marks(content::CapabilityHost& host,
                      const Module&,
                      const Inst&,
                      SlotTable&,
                      std::string*) {
  clear_marks(host);
  return true;
}

bool eval_select_view_tab(content::CapabilityHost& host,
                          const Module& module,
                          const Inst& inst,
                          SlotTable& slots,
                          std::string*) {
  select_view_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_catalog_tab(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string*) {
  catalog_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_inspector_tab(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string*) {
  inspector_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_suppress_dialogs(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error) {
  return ok_step(inst, suppress_dialogs(host, arg_bool_param(module, inst, slots, 0)),
                 error);
}

bool eval_require_hwnd(content::CapabilityHost& host,
                       const Module&,
                       const Inst& inst,
                       SlotTable&,
                       std::string* error) {
  return ok_step(inst, require_hwnd(host), error);
}

bool eval_window(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  return ok_step(inst,
                 window(host, arg_str_param(module, inst, slots, 0),
                            arg_i32_param(module, inst, slots, 1),
                            arg_i32_param(module, inst, slots, 2)),
                 error);
}

bool eval_key(content::CapabilityHost& host,
              const Module& module,
              const Inst& inst,
              SlotTable& slots,
              std::string* error) {
  return ok_step(inst, key(host, vk_of(host, module, inst, slots, 0)), error);
}

bool eval_key_edge(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  const unsigned vk = vk_of(host, module, inst, slots, 0);
  const bool down = arg_bool_param(module, inst, slots, 1);
  return ok_step(inst, post_key(host, vk, down), error);
}

bool eval_apply_ui_theme(content::CapabilityHost& host,
                         const Module&,
                         const Inst& inst,
                         SlotTable&,
                         std::string* error) {
  return ok_step(inst, apply_ui_theme(host), error);
}

bool eval_apply_scenario_panels(content::CapabilityHost& host,
                                const Module& module,
                                const Inst& inst,
                                SlotTable& slots,
                                std::string* error) {
  return ok_step(inst,
                 apply_scenario_panels(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_layout_gate(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst, layout_gate(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_ui_present_capture(content::CapabilityHost& host,
                             const Module& module,
                             const Inst& inst,
                             SlotTable& slots,
                             std::string* error) {
  return ok_step(inst,
                 ui_present_capture(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_console_pan_bench(content::CapabilityHost& host,
                            const Module&,
                            const Inst& inst,
                            SlotTable&,
                            std::string* error) {
  return ok_step(inst, console_pan_bench(host), error);
}

bool eval_expect_shell_tree(content::CapabilityHost& host,
                            const Module&,
                            const Inst& inst,
                            SlotTable&,
                            std::string* error) {
  return ok_step(inst, expect_shell_tree(host), error);
}

bool eval_expect_layout_bounds(content::CapabilityHost& host,
                               const Module&,
                               const Inst& inst,
                               SlotTable&,
                               std::string* error) {
  return ok_step(inst, expect_layout_bounds(host), error);
}

bool eval_wire_debug_agent(content::CapabilityHost& host,
                           const Module&,
                           const Inst& inst,
                           SlotTable&,
                           std::string* error) {
  return ok_step(inst, wire_debug_agent(host), error);
}

bool eval_debug_exec(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  return ok_step(inst,
                 debug_exec(host, arg_str_param(module, inst, slots, 0),
                                arg_str_param(module, inst, slots, 1),
                                arg_str_param(module, inst, slots, 2),
                                arg_str_param(module, inst, slots, 3),
                                arg_i32_param(module, inst, slots, 4)),
                 error);
}

constexpr Param kPumpParams[] = {
    {"ms", 0, ArgKind::kI32, false, 100, nullptr},
};
constexpr Param kMarkParams[] = {
    {"name", 0, ArgKind::kStr, false, 0, "mark"},
};
constexpr Param kIndexParams[] = {
    {"index", 0, ArgKind::kI32, false, 0, nullptr},
};
constexpr Param kSuppressParams[] = {
    {"on", 0, ArgKind::kBool, false, 1, nullptr},
};
constexpr Param kWindowParams[] = {
    {"action", 0, ArgKind::kStr, false, 0, "activate"},
    {"w", 1, ArgKind::kI32, false, 1280, nullptr},
    {"h", 2, ArgKind::kI32, false, 800, nullptr},
};
constexpr Param kKeyParams[] = {
    {"vk", 0, ArgKind::kVk, false, 0, ""},
};
constexpr Param kKeyEdgeParams[] = {
    {"vk", 0, ArgKind::kVk, false, 0, ""},
    {"down", 1, ArgKind::kBool, false, 1, nullptr},
};
constexpr Param kModeParams[] = {
    {"mode", 0, ArgKind::kStr, false, 0, "shell"},
};
constexpr Param kDebugParams[] = {
    {"line", 0, ArgKind::kStr, false, 0, ""},
    {"contains", -1, ArgKind::kStr, false, 0, ""},
    {"equals", -1, ArgKind::kStr, false, 0, ""},
    {"reject", -1, ArgKind::kStr, false, 0, ""},
    {"fail_rc", -1, ArgKind::kI32, false, 51, nullptr},
};

constexpr const char* const kDetachAliases[] = {"detach_maps", nullptr};
constexpr const char* const kSelectAliases[] = {"select_map_tab", nullptr};

CONTENT_LANE_OP(DetachViewsOp, Op::kDetachViews, "detach_views", kDetachAliases,
                nullptr, 0, false, eval_detach_views);
CONTENT_LANE_OP(PumpOp, Op::kPump, "pump", nullptr, kPumpParams, 1, false, eval_pump);
CONTENT_LANE_OP(MarkOp, Op::kMark, "mark", nullptr, kMarkParams, 1, false, eval_mark);
CONTENT_LANE_OP(ClearMarksOp, Op::kClearMarks, "clear_marks", nullptr, nullptr, 0,
                false, eval_clear_marks);
CONTENT_LANE_OP(SelectViewTabOp, Op::kSelectViewTab, "select_view_tab", kSelectAliases,
                kIndexParams, 1, false, eval_select_view_tab);
CONTENT_LANE_OP(CatalogTabOp, Op::kCatalogTab, "catalog_tab", nullptr, kIndexParams, 1,
                false, eval_catalog_tab);
CONTENT_LANE_OP(InspectorTabOp, Op::kInspectorTab, "inspector_tab", nullptr,
                kIndexParams, 1, false, eval_inspector_tab);
CONTENT_LANE_OP(SuppressDialogsOp, Op::kSuppressDialogs, "suppress_dialogs", nullptr,
                kSuppressParams, 1, false, eval_suppress_dialogs);
CONTENT_LANE_OP(RequireHwndOp, Op::kRequireHwnd, "require_hwnd", nullptr, nullptr, 0,
                false, eval_require_hwnd);
CONTENT_LANE_OP(WindowOp, Op::kWindow, "window", nullptr, kWindowParams, 3, false,
                eval_window);
CONTENT_LANE_OP(KeyOp, Op::kKey, "key", nullptr, kKeyParams, 1, false, eval_key);
CONTENT_LANE_OP(KeyEdgeOp, Op::kKeyEdge, "key_edge", nullptr, kKeyEdgeParams, 2, false,
                eval_key_edge);
CONTENT_LANE_OP(ApplyUiThemeOp, Op::kApplyUiTheme, "apply_ui_theme", nullptr, nullptr,
                0, false, eval_apply_ui_theme);
CONTENT_LANE_OP(ApplyScenarioPanelsOp, Op::kApplyScenarioPanels, "apply_scenario_panels",
                nullptr, kModeParams, 1, false, eval_apply_scenario_panels);
CONTENT_LANE_OP(LayoutGateOp, Op::kLayoutGate, "layout_gate", nullptr, kModeParams, 1,
                false, eval_layout_gate);
CONTENT_LANE_OP(UiPresentCaptureOp, Op::kUiPresentCapture, "ui_present_capture", nullptr,
                kModeParams, 1, false, eval_ui_present_capture);
CONTENT_LANE_OP(ConsolePanBenchOp, Op::kConsolePanBench, "console_pan_bench", nullptr,
                nullptr, 0, false, eval_console_pan_bench);
CONTENT_LANE_OP(ExpectShellTreeOp, Op::kExpectShellTree, "expect_shell_tree", nullptr,
                nullptr, 0, false, eval_expect_shell_tree);
CONTENT_LANE_OP(ExpectLayoutBoundsOp, Op::kExpectLayoutBounds, "expect_layout_bounds",
                nullptr, nullptr, 0, false, eval_expect_layout_bounds);
CONTENT_LANE_OP(WireDebugAgentOp, Op::kWireDebugAgent, "wire_debug_agent", nullptr,
                nullptr, 0, false, eval_wire_debug_agent);
CONTENT_LANE_OP(DebugExecOp, Op::kDebugExec, "debug_exec", nullptr, kDebugParams, 5,
                false, eval_debug_exec);

using HorizonOps = std::tuple<DetachViewsOp,
                              PumpOp,
                              MarkOp,
                              ClearMarksOp,
                              SelectViewTabOp,
                              CatalogTabOp,
                              InspectorTabOp,
                              SuppressDialogsOp,
                              RequireHwndOp,
                              WindowOp,
                              KeyOp,
                              KeyEdgeOp,
                              ApplyUiThemeOp,
                              ApplyScenarioPanelsOp,
                              LayoutGateOp,
                              UiPresentCaptureOp,
                              ConsolePanBenchOp,
                              ExpectShellTreeOp,
                              ExpectLayoutBoundsOp,
                              WireDebugAgentOp,
                              DebugExecOp>;

}  // namespace

void fill_horizon_ops(const OpDesc** by_op) {
  detail::install_ops<HorizonOps>(by_op);
}

}  // namespace code
}  // namespace content
