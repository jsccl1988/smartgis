// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BIND_SLOTS_H_
#define IL_RUNTIME_BIND_SLOTS_H_

#include <string_view>
#include <type_traits>
#include <utility>

#include "app/views/il.runtime/bind/reflect.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// NTTP lane + field. Slots live on a CapabilityHost lane, not the root.
template <auto Lane, auto Member>
struct host_member {
  static constexpr bool is_host_member = true;

  template <typename T>
  static void assign(content::CapabilityHost* out, T&& value) {
    if (!out) {
      return;
    }
    // Explicit Slot construction: MSVC /MDd has AVd when a returning lambda
    // is move-assigned into Host std::function members through tagged_tuple
    // decay alone (operator bool true, operator() never enters the body).
    using Slot = std::remove_cvref_t<decltype((out->*Lane).*Member)>;
    (out->*Lane).*Member = Slot(std::forward<T>(value));
  }
};

template <typename Tag>
concept host_member_tag = Tag::is_host_member;

// Assign a tagged pack onto the Host bag by reflecting each tagged field.
template <tagged_aggregate Pack>
void bind_tagged_slots(content::CapabilityHost* out, Pack&& pack) {
  if (!out) {
    return;
  }
  reflect_fields(std::forward<Pack>(pack), [&](auto&& item) {
    using Item = std::remove_cvref_t<decltype(item)>;
    using Tag = typename Item::tag_type;
    static_assert(host_member_tag<Tag>,
                  "tagged host pack tags must be host_member<&Lane, &Field>");
    Tag::assign(out, std::forward<decltype(item.value)>(item.value));
  });
}

// Lookup a named_tuple field by compile-time name string (mode tables, etc.).
template <named_aggregate Pack, typename T>
bool named_find(Pack&& pack, std::string_view key, T* out) {
  if (!out) {
    return false;
  }
  bool hit = false;
  reflect_fields(std::forward<Pack>(pack), [&](auto&& field) {
    if (hit || field.name.view() != key) {
      return;
    }
    using V = std::remove_cvref_t<decltype(field.value)>;
    if constexpr (std::is_convertible_v<V, T>) {
      *out = static_cast<T>(field.value);
      hit = true;
    }
  });
  return hit;
}

using Hz = content::HorizonCapability;
using Vw = content::ViewCapability;
using Pl = content::PluginCapability;
using Doc = content::DocumentCapability;
using Host = content::CapabilityHost;

using slot_pump = host_member<&Host::horizon, &Hz::pump>;
using slot_mark = host_member<&Host::horizon, &Hz::mark>;
using slot_select_map_tab = host_member<&Host::horizon, &Hz::select_map_tab>;
using slot_catalog_tab = host_member<&Host::horizon, &Hz::catalog_tab>;
using slot_inspector_tab = host_member<&Host::horizon, &Hz::inspector_tab>;
using slot_shell_hwnd = host_member<&Host::horizon, &Hz::shell_hwnd>;
using slot_hwnd_status = host_member<&Host::horizon, &Hz::hwnd_status>;
using slot_post_click = host_member<&Host::horizon, &Hz::post_click>;
using slot_post_drag = host_member<&Host::horizon, &Hz::post_drag>;
using slot_post_wheel = host_member<&Host::horizon, &Hz::post_wheel>;
using slot_post_path = host_member<&Host::horizon, &Hz::post_path>;
using slot_post_key = host_member<&Host::horizon, &Hz::post_key>;
using slot_vk_from_name = host_member<&Host::horizon, &Hz::vk_from_name>;
using slot_window = host_member<&Host::horizon, &Hz::window>;
using slot_key = host_member<&Host::horizon, &Hz::key>;
using slot_clear_marks = host_member<&Host::horizon, &Hz::clear_marks>;
using slot_suppress_dialogs = host_member<&Host::horizon, &Hz::suppress_dialogs>;
using slot_shell_status = host_member<&Host::horizon, &Hz::shell_status>;
using slot_layout_status = host_member<&Host::horizon, &Hz::layout_status>;
using slot_wire_debug_agent = host_member<&Host::horizon, &Hz::wire_debug_agent>;
using slot_debug_exec = host_member<&Host::horizon, &Hz::debug_exec>;
using slot_console_pan_bench = host_member<&Host::horizon, &Hz::console_pan_bench>;
using slot_apply_ui_theme = host_member<&Host::horizon, &Hz::apply_ui_theme>;
using slot_apply_scenario_panels =
    host_member<&Host::horizon, &Hz::apply_scenario_panels>;
using slot_layout_gate = host_member<&Host::horizon, &Hz::layout_gate>;
using slot_ui_present_capture =
    host_member<&Host::horizon, &Hz::ui_present_capture>;

using slot_dispatch_edit_input = host_member<&Host::view, &Vw::dispatch_edit_input>;
using slot_map_ready_status = host_member<&Host::view, &Vw::map_ready_status>;
using slot_detach_maps = host_member<&Host::view, &Vw::detach_maps>;
using slot_stop_map_present_timers =
    host_member<&Host::view, &Vw::stop_map_present_timers>;
using slot_resume_map_present_timers =
    host_member<&Host::view, &Vw::resume_map_present_timers>;
using slot_invalidate_map2d = host_member<&Host::view, &Vw::invalidate_map2d>;
using slot_edit_host_status = host_member<&Host::view, &Vw::edit_host_status>;
using slot_run_tool = host_member<&Host::view, &Vw::run_tool>;
using slot_tool_status = host_member<&Host::view, &Vw::tool_status>;
using slot_activate_tool = host_member<&Host::view, &Vw::activate_tool>;
using slot_last_geom = host_member<&Host::view, &Vw::last_geom>;
using slot_view_scale = host_member<&Host::view, &Vw::view_scale>;
using slot_load_status = host_member<&Host::view, &Vw::load_status>;
using slot_browse_stress = host_member<&Host::view, &Vw::browse_stress>;
using slot_fps_bench = host_member<&Host::view, &Vw::fps_bench>;
using slot_capture_browse_still =
    host_member<&Host::view, &Vw::capture_browse_still>;
using slot_fit_scene_box = host_member<&Host::view, &Vw::fit_scene_box>;
using slot_camera_fly = host_member<&Host::view, &Vw::camera_fly>;

using slot_map2d_run = host_member<&Host::plugin, &Pl::map2d_run>;
using slot_atmosphere_run = host_member<&Host::plugin, &Pl::atmosphere_run>;
using slot_run_plugin_command = host_member<&Host::plugin, &Pl::run_plugin_command>;
using slot_run_processing = host_member<&Host::plugin, &Pl::run_processing>;
using slot_require_plugins = host_member<&Host::plugin, &Pl::require_plugins>;
using slot_resolve_data = host_member<&Host::plugin, &Pl::resolve_data>;
using slot_capture_path = host_member<&Host::plugin, &Pl::capture_path>;
using slot_sidecar_path = host_member<&Host::plugin, &Pl::sidecar_path>;
using slot_analysis_set_frame = host_member<&Host::plugin, &Pl::analysis_set_frame>;
using slot_analysis_export_frames =
    host_member<&Host::plugin, &Pl::analysis_export_frames>;
using slot_open_report = host_member<&Host::plugin, &Pl::open_report>;
using slot_post_to_report = host_member<&Host::plugin, &Pl::post_to_report>;

using slot_open_map = host_member<&Host::document, &Doc::open_map>;
using slot_doc_clear = host_member<&Host::document, &Doc::doc_clear>;
using slot_fit_extent = host_member<&Host::document, &Doc::fit_extent>;
using slot_export_bmp = host_member<&Host::document, &Doc::export_bmp>;
using slot_apply_style_file = host_member<&Host::document, &Doc::apply_style_file>;

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BIND_SLOTS_H_
