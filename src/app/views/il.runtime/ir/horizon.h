// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_HORIZON_H_
#define IL_RUNTIME_IR_HORIZON_H_

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Shell clock, tabs, marks, and HWND. No script AST.

inline void pump(content::CapabilityHost& host, int ms) {
  if (host.pump) {
    host.pump(ms);
  }
}

inline void mark(content::CapabilityHost& host, std::string_view token) {
  if (host.mark) {
    host.mark(std::string(token));
  }
}

inline void select_map_tab(content::CapabilityHost& host, int index) {
  if (host.select_map_tab) {
    host.select_map_tab(index);
  }
}

inline void catalog_tab(content::CapabilityHost& host, int index) {
  if (host.catalog_tab) {
    host.catalog_tab(index);
  }
}

inline void inspector_tab(content::CapabilityHost& host, int index) {
  if (host.inspector_tab) {
    host.inspector_tab(index);
  }
}

inline bool wait_map_ready(content::CapabilityHost& host, int timeout_ms) {
  return host.wait_map_ready && host.wait_map_ready(timeout_ms);
}

// Missing HWND detaches maps and records exit code 2.
inline bool require_hwnd(content::CapabilityHost& host) {
  if (host.require_hwnd && host.require_hwnd()) {
    return true;
  }
  if (host.detach_maps) {
    host.detach_maps();
  }
  host.fail_rc = 2;
  return false;
}

inline bool edit_host_ready(content::CapabilityHost& host) {
  return host.edit_host_ready && host.edit_host_ready();
}

inline bool window(content::CapabilityHost& host,
                   std::string_view action,
                   int w,
                   int h) {
  return host.window && host.window(std::string(action), w, h);
}

inline unsigned vk_from_name(content::CapabilityHost& host,
                             std::string_view name) {
  if (!host.vk_from_name || name.empty()) {
    return 0;
  }
  return host.vk_from_name(std::string(name));
}

inline bool key(content::CapabilityHost& host, unsigned vk) {
  return host.key && host.key(vk);
}

inline bool post_key(content::CapabilityHost& host, unsigned vk, bool down) {
  return host.post_key && host.post_key(vk, down);
}

// Non-zero gate codes fail the step and stick on CapabilityHost::fail_rc.
inline bool apply_rc(content::CapabilityHost& host, int rc) {
  if (rc != 0) {
    host.fail_rc = rc;
    return false;
  }
  return true;
}

inline bool note_fail(content::CapabilityHost& host, bool ok, int fail_rc) {
  if (!ok && fail_rc != 0) {
    host.fail_rc = fail_rc;
  }
  return ok;
}

inline bool browse_stress(content::CapabilityHost& host, int count) {
  return host.browse_stress && host.browse_stress(count);
}

inline bool apply_ui_theme(content::CapabilityHost& host) {
  return host.apply_ui_theme && host.apply_ui_theme();
}

inline bool ensure_china_map(content::CapabilityHost& host,
                             std::string_view mode) {
  return host.ensure_china_map && host.ensure_china_map(std::string(mode));
}

inline bool apply_scenario_panels(content::CapabilityHost& host,
                                  std::string_view mode) {
  return host.apply_scenario_panels &&
         host.apply_scenario_panels(std::string(mode));
}

inline bool layout_gate(content::CapabilityHost& host, std::string_view mode) {
  if (!host.layout_gate) {
    return false;
  }
  return apply_rc(host, host.layout_gate(std::string(mode)));
}

inline bool ui_present_capture(content::CapabilityHost& host,
                               std::string_view mode) {
  if (!host.ui_present_capture) {
    return false;
  }
  return apply_rc(host, host.ui_present_capture(std::string(mode)));
}

inline bool fps_bench(content::CapabilityHost& host) {
  return host.fps_bench && host.fps_bench();
}

inline bool capture_shell_bmp(content::CapabilityHost& host,
                              std::string_view leaf) {
  return host.capture_shell_bmp && host.capture_shell_bmp(std::string(leaf));
}

// Browse still ("map2d" or "scene3d"). The binder records a miss and returns
// true for a known face so a soft BMP miss does not abort the suite.
inline bool capture_browse_still(content::CapabilityHost& host,
                                 std::string_view face) {
  return host.capture_browse_still &&
         host.capture_browse_still(std::string(face));
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_HORIZON_H_
