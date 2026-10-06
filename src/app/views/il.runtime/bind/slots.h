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

// NTTP member-pointer tag: the trait *is* the CapabilityHost field.
template <auto Member>
struct host_member {
  static constexpr auto member = Member;
};

template <typename Tag>
concept host_member_tag = requires {
  Tag::member;
};

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
                  "tagged host pack tags must be host_member<&CapabilityHost::…>");
    (*out).*Tag::member = std::forward<decltype(item.value)>(item.value);
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

using slot_pump = host_member<&content::CapabilityHost::pump>;
using slot_mark = host_member<&content::CapabilityHost::mark>;
using slot_select_map_tab =
    host_member<&content::CapabilityHost::select_map_tab>;
using slot_catalog_tab = host_member<&content::CapabilityHost::catalog_tab>;
using slot_inspector_tab =
    host_member<&content::CapabilityHost::inspector_tab>;
using slot_shell_hwnd = host_member<&content::CapabilityHost::shell_hwnd>;
using slot_require_hwnd =
    host_member<&content::CapabilityHost::require_hwnd>;
using slot_post_click = host_member<&content::CapabilityHost::post_click>;
using slot_post_drag = host_member<&content::CapabilityHost::post_drag>;
using slot_post_wheel = host_member<&content::CapabilityHost::post_wheel>;
using slot_post_path = host_member<&content::CapabilityHost::post_path>;
using slot_post_key = host_member<&content::CapabilityHost::post_key>;
using slot_vk_from_name =
    host_member<&content::CapabilityHost::vk_from_name>;
using slot_dispatch_edit_input =
    host_member<&content::CapabilityHost::dispatch_edit_input>;
using slot_wait_map_ready =
    host_member<&content::CapabilityHost::wait_map_ready>;
using slot_load_china_sample =
    host_member<&content::CapabilityHost::load_china_sample>;
using slot_detach_maps = host_member<&content::CapabilityHost::detach_maps>;
using slot_stop_map_present_timers =
    host_member<&content::CapabilityHost::stop_map_present_timers>;
using slot_window = host_member<&content::CapabilityHost::window>;
using slot_key = host_member<&content::CapabilityHost::key>;
using slot_clear_marks = host_member<&content::CapabilityHost::clear_marks>;
using slot_edit_host_ready =
    host_member<&content::CapabilityHost::edit_host_ready>;
using slot_run_tool = host_member<&content::CapabilityHost::run_tool>;
using slot_current_tool_id =
    host_member<&content::CapabilityHost::current_tool_id>;
using slot_expect_last_geom =
    host_member<&content::CapabilityHost::expect_last_geom>;
using slot_browse_stress =
    host_member<&content::CapabilityHost::browse_stress>;
using slot_expect_wheel_cursor =
    host_member<&content::CapabilityHost::expect_wheel_cursor>;
using slot_map2d_run = host_member<&content::CapabilityHost::map2d_run>;
using slot_atmosphere_run =
    host_member<&content::CapabilityHost::atmosphere_run>;
using slot_run_plugin_command =
    host_member<&content::CapabilityHost::run_plugin_command>;
using slot_run_processing =
    host_member<&content::CapabilityHost::run_processing>;
using slot_resolve_data = host_member<&content::CapabilityHost::resolve_data>;
using slot_capture_path = host_member<&content::CapabilityHost::capture_path>;
using slot_sidecar_path = host_member<&content::CapabilityHost::sidecar_path>;
using slot_doc_clear = host_member<&content::CapabilityHost::doc_clear>;
using slot_fit_extent = host_member<&content::CapabilityHost::fit_extent>;
using slot_export_bmp = host_member<&content::CapabilityHost::export_bmp>;
using slot_suppress_dialogs =
    host_member<&content::CapabilityHost::suppress_dialogs>;
using slot_require_plugins =
    host_member<&content::CapabilityHost::require_plugins>;
using slot_apply_style_file =
    host_member<&content::CapabilityHost::apply_style_file>;
using slot_open_map = host_member<&content::CapabilityHost::open_map>;
using slot_invalidate_map2d =
    host_member<&content::CapabilityHost::invalidate_map2d>;
using slot_analysis_set_frame =
    host_member<&content::CapabilityHost::analysis_set_frame>;
using slot_analysis_export_frames =
    host_member<&content::CapabilityHost::analysis_export_frames>;
using slot_open_report = host_member<&content::CapabilityHost::open_report>;
using slot_post_to_report =
    host_member<&content::CapabilityHost::post_to_report>;
using slot_resume_map_present_timers =
    host_member<&content::CapabilityHost::resume_map_present_timers>;
using slot_wait_viewport =
    host_member<&content::CapabilityHost::wait_viewport>;
using slot_expect_shell_tree =
    host_member<&content::CapabilityHost::expect_shell_tree>;
using slot_expect_scene_visible =
    host_member<&content::CapabilityHost::expect_scene_visible>;
using slot_expect_orbit_moved =
    host_member<&content::CapabilityHost::expect_orbit_moved>;
using slot_expect_layout_bounds =
    host_member<&content::CapabilityHost::expect_layout_bounds>;
using slot_expect_map_hwnd_sync =
    host_member<&content::CapabilityHost::expect_map_hwnd_sync>;
using slot_activate_tool =
    host_member<&content::CapabilityHost::activate_tool>;
using slot_wire_debug_agent =
    host_member<&content::CapabilityHost::wire_debug_agent>;
using slot_debug_exec = host_member<&content::CapabilityHost::debug_exec>;
using slot_console_pan_bench =
    host_member<&content::CapabilityHost::console_pan_bench>;
using slot_apply_ui_theme =
    host_member<&content::CapabilityHost::apply_ui_theme>;
using slot_ensure_china_map =
    host_member<&content::CapabilityHost::ensure_china_map>;
using slot_apply_scenario_panels =
    host_member<&content::CapabilityHost::apply_scenario_panels>;
using slot_layout_gate = host_member<&content::CapabilityHost::layout_gate>;
using slot_ui_present_capture =
    host_member<&content::CapabilityHost::ui_present_capture>;
using slot_fps_bench = host_member<&content::CapabilityHost::fps_bench>;
using slot_capture_shell_bmp =
    host_member<&content::CapabilityHost::capture_shell_bmp>;
using slot_capture_browse_still =
    host_member<&content::CapabilityHost::capture_browse_still>;

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BIND_SLOTS_H_
