// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/view/view_bind.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

#include "content/browser/glang.runtime/host/document/document.h"
#include "content/browser/glang.runtime/host/view/host/capture_host.h"
#include "content/browser/glang.runtime/host/view/probe.h"
#include "content/browser/glang.runtime/host/horizon/sema/expect.h"
#include "content/browser/glang.runtime/host/scene/bind.h"
#include "content/browser/glang.runtime/host/bind/slots.h"

namespace content {
namespace detail {

void register_view(content::Shell& browser,
                   content::CapabilityHost* out,
                   const wchar_t* mark_leaf) {
  content::Shell* b = &browser;
  const wchar_t* leaf = mark_leaf;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_dispatch_edit_input> =
                   [b](const content::InputEvent& e) {
                     return dispatch_edit_input(*b, e);
                   },
               base::tag_resolver<slot_detach_views> =
                   [b]() { detach_views(*b); },
               base::tag_resolver<slot_stop_present_timers> =
                   [b]() { stop_present_timers(*b); },
               base::tag_resolver<slot_resume_present_timers> =
                   [b]() { resume_present_timers(*b); },
               base::tag_resolver<slot_run_tool> =
                   [b](const std::string& command_id) {
                     return b->run_tool_command(command_id);
                   },
               base::tag_resolver<slot_load_status> =
                   [b](const std::string& face, int timeout_ms,
                       content::ViewLoadStatus* status) {
                     return fill_map_load_status(*b, face, timeout_ms, status);
                   },
               base::tag_resolver<slot_map_ready_status> =
                   [b](int timeout_ms, content::MapReadyStatus* status) {
                     return fill_map_ready_status(*b, timeout_ms, status);
                   },
               base::tag_resolver<slot_edit_tool_session_status> =
                   [b](content::EditToolSessionStatus* status) {
                     return fill_edit_tool_session_status(*b, status);
                   },
               base::tag_resolver<slot_tool_status> =
                   [b](content::ToolStatus* status) {
                     return fill_tool_status(*b, status);
                   },
               base::tag_resolver<slot_last_geom> =
                   [b](content::GeomStatus* status) {
                     return fill_last_geom(*b, status);
                   },
               base::tag_resolver<slot_view_scale> =
                   [b](content::ViewScaleStatus* status) {
                     return fill_view_scale(*b, status);
                   },
               base::tag_resolver<slot_activate_tool> =
                   [b](const std::string& id) { return activate_view_tool(*b, id); },
           });
  register_scene_slots(browser, out, leaf);
}

}  // namespace detail
}  // namespace content
