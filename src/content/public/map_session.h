// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_SESSION_H
#define CONTENT_PUBLIC_MAP_SESSION_H

#include <cstddef>
#include <cstdint>

#include "content/public/map_types.h"
#include "content/public/map_view.h"
#include "content/public/tool_router.h"

// App-process session: relaunch this PE with --type=gpu, N MapView
// surfaces, fire-and-forget extent / input. Hosts include only
// content/public -- not sdb, SmtGisCore, or render device headers.
namespace content {

class MapSessionClient {
 public:
  virtual ~MapSessionClient() = default;
  virtual void on_frame_ready(uint32_t view_id, uint32_t generation) {}
  virtual void on_extent_changed(uint32_t view_id, const Extent2& e) {}
  virtual void on_render_died() {}
};

class MapSession {
 public:
  virtual ~MapSession() = default;

  virtual bool start_render_process() = 0;
  virtual void shutdown() = 0;
  virtual bool is_oop_render() const = 0;
  virtual const wchar_t* present_status() const = 0;

  virtual uint32_t open_view(ViewKind kind) = 0;
  virtual void close_view(uint32_t view_id) = 0;
  virtual MapView* attach_surface(uint32_t view_id, PresentMode mode) = 0;
  virtual MapView* map_view(uint32_t view_id) = 0;

  virtual void set_extent(uint32_t view_id, const Extent2& e) = 0;
  virtual Extent2 extent(uint32_t view_id) const = 0;

  virtual void set_selection(uint32_t view_id,
                             const FeatureId* ids,
                             size_t n) = 0;
  virtual void legend_snapshot(uint32_t view_id) = 0;
  virtual void catalog_call(const char* json_op) = 0;

  virtual void activate_tool(uint32_t view_id, const char* tool_id) = 0;
  virtual void dispatch(uint32_t view_id, const InputEvent& e) = 0;

  virtual ToolRouter* tool_router() = 0;
  virtual void set_client(MapSessionClient* client) = 0;
  virtual bool wait_frame_ready(uint32_t view_id, uint32_t timeout_ms) = 0;
};

// UI / chrome process entry. Caller deletes the session after shutdown().
MapSession* create_map_session();

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_SESSION_H
