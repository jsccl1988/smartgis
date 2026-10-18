// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"

#include <cstdlib>
#include <string>

#include "content/browser/glang.runtime/language/ir/horizon.h"
#include "content/browser/glang.runtime/language/ir/view.h"

namespace content {
namespace code {
namespace {

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
  unsigned vk = ir::vk_from_name(host, name);
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
  ir::detach_views(host);
  return true;
}

bool eval_pump(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string*) {
  ir::pump(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_mark(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string*) {
  ir::mark(host, arg_str_param(module, inst, slots, 0));
  return true;
}

bool eval_clear_marks(content::CapabilityHost& host,
                      const Module&,
                      const Inst&,
                      SlotTable&,
                      std::string*) {
  ir::clear_marks(host);
  return true;
}

bool eval_select_view_tab(content::CapabilityHost& host,
                          const Module& module,
                          const Inst& inst,
                          SlotTable& slots,
                          std::string*) {
  ir::select_view_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_catalog_tab(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string*) {
  ir::catalog_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_inspector_tab(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string*) {
  ir::inspector_tab(host, arg_i32_param(module, inst, slots, 0));
  return true;
}

bool eval_suppress_dialogs(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error) {
  return ok_step(inst, ir::suppress_dialogs(host, arg_bool_param(module, inst, slots, 0)),
                 error);
}

bool eval_require_hwnd(content::CapabilityHost& host,
                       const Module&,
                       const Inst& inst,
                       SlotTable&,
                       std::string* error) {
  return ok_step(inst, ir::require_hwnd(host), error);
}

bool eval_window(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  return ok_step(inst,
                 ir::window(host, arg_str_param(module, inst, slots, 0),
                            arg_i32_param(module, inst, slots, 1),
                            arg_i32_param(module, inst, slots, 2)),
                 error);
}

bool eval_key(content::CapabilityHost& host,
              const Module& module,
              const Inst& inst,
              SlotTable& slots,
              std::string* error) {
  return ok_step(inst, ir::key(host, vk_of(host, module, inst, slots, 0)), error);
}

bool eval_key_edge(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  const unsigned vk = vk_of(host, module, inst, slots, 0);
  const bool down = arg_bool_param(module, inst, slots, 1);
  return ok_step(inst, ir::post_key(host, vk, down), error);
}

bool eval_apply_ui_theme(content::CapabilityHost& host,
                         const Module&,
                         const Inst& inst,
                         SlotTable&,
                         std::string* error) {
  return ok_step(inst, ir::apply_ui_theme(host), error);
}

bool eval_apply_scenario_panels(content::CapabilityHost& host,
                                const Module& module,
                                const Inst& inst,
                                SlotTable& slots,
                                std::string* error) {
  return ok_step(inst,
                 ir::apply_scenario_panels(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_layout_gate(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst, ir::layout_gate(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_ui_present_capture(content::CapabilityHost& host,
                             const Module& module,
                             const Inst& inst,
                             SlotTable& slots,
                             std::string* error) {
  return ok_step(inst,
                 ir::ui_present_capture(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_console_pan_bench(content::CapabilityHost& host,
                            const Module&,
                            const Inst& inst,
                            SlotTable&,
                            std::string* error) {
  return ok_step(inst, ir::console_pan_bench(host), error);
}

bool eval_expect_shell_tree(content::CapabilityHost& host,
                            const Module&,
                            const Inst& inst,
                            SlotTable&,
                            std::string* error) {
  return ok_step(inst, ir::expect_shell_tree(host), error);
}

bool eval_expect_layout_bounds(content::CapabilityHost& host,
                               const Module&,
                               const Inst& inst,
                               SlotTable&,
                               std::string* error) {
  return ok_step(inst, ir::expect_layout_bounds(host), error);
}

bool eval_wire_debug_agent(content::CapabilityHost& host,
                           const Module&,
                           const Inst& inst,
                           SlotTable&,
                           std::string* error) {
  return ok_step(inst, ir::wire_debug_agent(host), error);
}

bool eval_debug_exec(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  return ok_step(inst,
                 ir::debug_exec(host, arg_str_param(module, inst, slots, 0),
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

const OpDesc kDetachViews{
    Op::kDetachViews, "detach_views", kDetachAliases, nullptr, 0, false,
    &eval_detach_views,
};
const OpDesc kPump{Op::kPump, "pump", nullptr, kPumpParams, 1, false, &eval_pump};
const OpDesc kMark{Op::kMark, "mark", nullptr, kMarkParams, 1, false, &eval_mark};
const OpDesc kClearMarks{
    Op::kClearMarks, "clear_marks", nullptr, nullptr, 0, false, &eval_clear_marks,
};
const OpDesc kSelectViewTab{
    Op::kSelectViewTab, "select_view_tab", kSelectAliases, kIndexParams, 1, false,
    &eval_select_view_tab,
};
const OpDesc kCatalogTab{
    Op::kCatalogTab, "catalog_tab", nullptr, kIndexParams, 1, false, &eval_catalog_tab,
};
const OpDesc kInspectorTab{
    Op::kInspectorTab, "inspector_tab", nullptr, kIndexParams, 1, false,
    &eval_inspector_tab,
};
const OpDesc kSuppressDialogs{
    Op::kSuppressDialogs, "suppress_dialogs", nullptr, kSuppressParams, 1, false,
    &eval_suppress_dialogs,
};
const OpDesc kRequireHwnd{
    Op::kRequireHwnd, "require_hwnd", nullptr, nullptr, 0, false, &eval_require_hwnd,
};
const OpDesc kWindow{
    Op::kWindow, "window", nullptr, kWindowParams, 3, false, &eval_window,
};
const OpDesc kKey{Op::kKey, "key", nullptr, kKeyParams, 1, false, &eval_key};
const OpDesc kKeyEdge{
    Op::kKeyEdge, "key_edge", nullptr, kKeyEdgeParams, 2, false, &eval_key_edge,
};
const OpDesc kApplyUiTheme{
    Op::kApplyUiTheme, "apply_ui_theme", nullptr, nullptr, 0, false,
    &eval_apply_ui_theme,
};
const OpDesc kApplyScenarioPanels{
    Op::kApplyScenarioPanels, "apply_scenario_panels", nullptr, kModeParams, 1, false,
    &eval_apply_scenario_panels,
};
const OpDesc kLayoutGate{
    Op::kLayoutGate, "layout_gate", nullptr, kModeParams, 1, false, &eval_layout_gate,
};
const OpDesc kUiPresentCapture{
    Op::kUiPresentCapture, "ui_present_capture", nullptr, kModeParams, 1, false,
    &eval_ui_present_capture,
};
const OpDesc kConsolePanBench{
    Op::kConsolePanBench, "console_pan_bench", nullptr, nullptr, 0, false,
    &eval_console_pan_bench,
};
const OpDesc kExpectShellTree{
    Op::kExpectShellTree, "expect_shell_tree", nullptr, nullptr, 0, false,
    &eval_expect_shell_tree,
};
const OpDesc kExpectLayoutBounds{
    Op::kExpectLayoutBounds, "expect_layout_bounds", nullptr, nullptr, 0, false,
    &eval_expect_layout_bounds,
};
const OpDesc kWireDebugAgent{
    Op::kWireDebugAgent, "wire_debug_agent", nullptr, nullptr, 0, false,
    &eval_wire_debug_agent,
};
const OpDesc kDebugExec{
    Op::kDebugExec, "debug_exec", nullptr, kDebugParams, 5, false, &eval_debug_exec,
};

}  // namespace

void fill_horizon_ops(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  by_op[static_cast<std::size_t>(Op::kDetachViews)] = &kDetachViews;
  by_op[static_cast<std::size_t>(Op::kPump)] = &kPump;
  by_op[static_cast<std::size_t>(Op::kMark)] = &kMark;
  by_op[static_cast<std::size_t>(Op::kClearMarks)] = &kClearMarks;
  by_op[static_cast<std::size_t>(Op::kSelectViewTab)] = &kSelectViewTab;
  by_op[static_cast<std::size_t>(Op::kCatalogTab)] = &kCatalogTab;
  by_op[static_cast<std::size_t>(Op::kInspectorTab)] = &kInspectorTab;
  by_op[static_cast<std::size_t>(Op::kSuppressDialogs)] = &kSuppressDialogs;
  by_op[static_cast<std::size_t>(Op::kRequireHwnd)] = &kRequireHwnd;
  by_op[static_cast<std::size_t>(Op::kWindow)] = &kWindow;
  by_op[static_cast<std::size_t>(Op::kKey)] = &kKey;
  by_op[static_cast<std::size_t>(Op::kKeyEdge)] = &kKeyEdge;
  by_op[static_cast<std::size_t>(Op::kApplyUiTheme)] = &kApplyUiTheme;
  by_op[static_cast<std::size_t>(Op::kApplyScenarioPanels)] = &kApplyScenarioPanels;
  by_op[static_cast<std::size_t>(Op::kLayoutGate)] = &kLayoutGate;
  by_op[static_cast<std::size_t>(Op::kUiPresentCapture)] = &kUiPresentCapture;
  by_op[static_cast<std::size_t>(Op::kConsolePanBench)] = &kConsolePanBench;
  by_op[static_cast<std::size_t>(Op::kExpectShellTree)] = &kExpectShellTree;
  by_op[static_cast<std::size_t>(Op::kExpectLayoutBounds)] = &kExpectLayoutBounds;
  by_op[static_cast<std::size_t>(Op::kWireDebugAgent)] = &kWireDebugAgent;
  by_op[static_cast<std::size_t>(Op::kDebugExec)] = &kDebugExec;
}

}  // namespace code
}  // namespace content
