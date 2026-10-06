// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/lower/horizon.h"

#include <string>

#include "app/views/il.runtime/codegen/lower/act.h"
#include "app/views/il.runtime/codegen/lower/ops.h"
#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {
namespace {

template <auto Fn>
std::optional<Action> lower_index(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("index"_t = 0),
                     [](content::CapabilityHost& host, const auto& p) {
                       Fn(host, p["index"_t]);
                       return true;
                     });
}

template <auto Fn>
std::optional<Action> lower_mode(const CallStmt& c, VarMap*) {
  return bind_action(
      c, make_named_tuple("mode"_t = std::string("shell")),
      [](content::CapabilityHost& host, const auto& p) {
        return Fn(host, p["mode"_t]);
      });
}

std::optional<Action> lower_pump(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("ms"_t = 100),
                     [](content::CapabilityHost& host, const auto& p) {
                       ir::pump(host, p["ms"_t]);
                       return true;
                     });
}

std::optional<Action> lower_mark(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("name"_t = std::string("mark")),
                     [](content::CapabilityHost& host, const auto& p) {
                       ir::mark(host, p["name"_t]);
                       return true;
                     });
}

std::optional<Action> lower_suppress_dialogs(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("on"_t = 1),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::suppress_dialogs(host, p["on"_t] != 0);
                     });
}

std::optional<Action> lower_window(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("action"_t = std::string("activate"), "w"_t = 1280,
                       "h"_t = 800),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::window(host, p["action"_t], p["w"_t], p["h"_t]);
      });
}

std::optional<Action> lower_key(const CallStmt& c, VarMap*) {
  auto p = bind_pack(c, make_named_tuple("vk"_t = std::string()));
  // A number and a key name share the first argument. The name wins when it maps.
  const int vk_num = arg_int(c, 0, "vk", 0);
  return host_action(std::move(p), [vk_num](content::CapabilityHost& host,
                                            const auto& pack) {
    unsigned vk = ir::vk_from_name(host, pack["vk"_t]);
    if (!vk) {
      vk = static_cast<unsigned>(vk_num);
    }
    return ir::key(host, vk);
  });
}

std::optional<Action> lower_shell(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_pump, "pump"),
      op(lower_index<ir::select_map_tab>, "select_map_tab"),
      op(lower_index<ir::catalog_tab>, "catalog_tab"),
      op(lower_index<ir::inspector_tab>, "inspector_tab"),
      op(lower_mark, "mark"),
      op(lower_host<ir::clear_marks>, "clear_marks"),
      op(lower_suppress_dialogs, "suppress_dialogs"),
      op(lower_host<ir::require_hwnd>, "require_hwnd"),
      op(lower_window, "window"), op(lower_key, "key"));
}

std::optional<Action> lower_expect_shell_tree(const CallStmt&, VarMap*) {
  // Prefer an explicit Action over lower_host<&ir::expect_shell_tree>: the
  // NTTP form has AVd on operator() under harness while this path is stable.
  return Action(
      [](content::CapabilityHost& host) { return ir::expect_shell_tree(host); });
}

std::optional<Action> lower_scenario(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_host<ir::apply_ui_theme>, "apply_ui_theme"),
      op(lower_mode<ir::apply_scenario_panels>, "apply_scenario_panels"),
      op(lower_mode<ir::layout_gate>, "layout_gate"),
      op(lower_mode<ir::ui_present_capture>, "ui_present_capture"),
      op(lower_host<ir::console_pan_bench>, "console_pan_bench"),
      op(lower_expect_shell_tree, "expect_shell_tree"),
      op(lower_host<ir::expect_layout_bounds>, "expect_layout_bounds"));
}

std::optional<Action> lower_debug_exec(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("line"_t = std::string(),
                       named_only<"contains"> = std::string(),
                       named_only<"equals"> = std::string(),
                       named_only<"reject"> = std::string(),
                       named_only<"fail_rc"> = 51),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::debug_exec(host, p["line"_t], p["contains"_t], p["equals"_t],
                              p["reject"_t], p["fail_rc"_t]);
      });
}

std::optional<Action> lower_debug(const CallStmt& c, VarMap* vars) {
  return lower_ops(c, vars,
                   op(lower_host<ir::wire_debug_agent>, "wire_debug_agent"),
                   op(lower_debug_exec, "debug_exec"));
}

}  // namespace

std::optional<Action> try_lower_horizon_call(const CallStmt& c, VarMap* vars) {
  if (std::optional<Action> hit = lower_shell(c, vars)) {
    return hit;
  }
  if (std::optional<Action> hit = lower_scenario(c, vars)) {
    return hit;
  }
  return lower_debug(c, vars);
}

}  // namespace detail
}  // namespace app
