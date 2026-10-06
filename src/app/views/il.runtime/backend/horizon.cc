// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon.h"

#include <string>

#include "app/views/il.runtime/backend/act.h"
#include "app/views/il.runtime/backend/ops.h"
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

std::optional<Action> lower_wait_ready(const CallStmt& c, VarMap*) {
  return bind_action(
      c, make_named_tuple("ms"_t = 5000, named_only<"soft"> = 0),
      [](content::CapabilityHost& host, const auto& p) {
        const bool ok = ir::wait_map_ready(host, p["ms"_t]);
        return p["soft"_t] ? true : ok;
      });
}

std::optional<Action> lower_wait_viewport(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("face"_t = std::string("map"), "ms"_t = 90000,
                       named_only<"frame"> = 1),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::wait_viewport(host, p["face"_t], p["ms"_t], p["frame"_t]);
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
      op(lower_wait_ready, "wait_ready", "wait_map_ready"),
      op(lower_wait_viewport, "wait_viewport"),
      op(lower_host<ir::require_hwnd>, "require_hwnd"),
      op(lower_host<ir::edit_host_ready>, "require_edit_host", "expect_host"),
      op(lower_window, "window"), op(lower_key, "key"));
}

std::optional<Action> lower_browse_stress(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("count"_t = 24),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::browse_stress(host, p["count"_t]);
                     });
}

std::optional<Action> lower_scenario(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_browse_stress, "browse_stress"),
      op(lower_host<ir::apply_ui_theme>, "apply_ui_theme"),
      op(lower_mode<ir::ensure_china_map>, "ensure_china_map"),
      op(lower_mode<ir::apply_scenario_panels>, "apply_scenario_panels"),
      op(lower_mode<ir::layout_gate>, "layout_gate"));
}

std::optional<Action> lower_capture_shell_bmp(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("leaf"_t = std::string()),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::capture_shell_bmp(host, p["leaf"_t]);
                     });
}

std::optional<Action> lower_capture_browse_still(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("face"_t = std::string("map2d")),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::capture_browse_still(host, p["face"_t]);
                     });
}

std::optional<Action> lower_capture(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_mode<ir::ui_present_capture>, "ui_present_capture"),
      op(lower_host<ir::fps_bench>, "fps_bench"),
      op(lower_host<ir::console_pan_bench>, "console_pan_bench"),
      op(lower_capture_shell_bmp, "capture_shell_bmp"),
      op(lower_capture_browse_still, "capture_browse_still"));
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
  if (std::optional<Action> hit = lower_capture(c, vars)) {
    return hit;
  }
  return lower_debug(c, vars);
}

}  // namespace detail
}  // namespace app
