// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/map_session.h"

#include <cstdlib>
#include <cstring>

#include "base/trace/event/process_trace.h"
#include "content/public/map_contents.h"
#include "content/public/map_contents_observer.h"
#include "content/public/view_host.h"

namespace content {
namespace {

bool env_flag_on(const char* name) {
  const char* env = std::getenv(name);
  return env && env[0] == '1' && env[1] == '\0';
}

// Explicit opt-in to start OOP GPU at init_hosts (legacy / debug).
// Default is delay until ensure_oop_render_process().
bool want_oop_at_init() {
  if (env_flag_on("SMT_DISABLE_OOP_RENDER")) {
    return false;
  }
  return env_flag_on("SMT_ENABLE_OOP_RENDER");
}

}  // namespace

MapSession::MapSession() = default;

MapSession::~MapSession() {
  prepare_close();
  clear_map_contents_observer();
}

void MapSession::init_hosts() {
  edit_host_ = std::make_unique<ViewHost>();
  data_host_ = std::make_unique<ViewHost>();
  scene_host_ = std::make_unique<ViewHost>();
  // OOP MapContents is optional for in-process present (atmosphere / map2d
  // showcase). Create the session object eagerly; StartRenderProcess is
  // deferred until ensure_oop_render_process() (or SMT_ENABLE_OOP_RENDER=1).
  {
    BASE_TRACE_EVENT("MapContents.Create", "startup");
    map_contents_.reset(MapContents::Create());
  }
  if (map_contents_ && want_oop_at_init()) {
    if (!ensure_oop_render_process()) {
      map_contents_.reset();
    }
  }
}

bool MapSession::ensure_oop_render_process() {
  if (!map_contents_) {
    BASE_TRACE_EVENT("MapContents.Create", "startup");
    map_contents_.reset(MapContents::Create());
    if (!map_contents_) {
      return false;
    }
  }
  if (map_contents_->IsOopRender()) {
    return true;
  }
  if (env_flag_on("SMT_DISABLE_OOP_RENDER")) {
    return false;
  }
  bool ok = false;
  {
    BASE_TRACE_EVENT("StartRenderProcess", "startup");
    ok = map_contents_->StartRenderProcess();
  }
  if (!ok) {
    // Keep the MapContents object for CatalogCall no-ops; callers that need
    // a live pipe check IsOopRender(). Dropping here matches historical
    // init_hosts failure (clear session) only when Create was for OOP-at-init.
    return false;
  }
  return true;
}

void MapSession::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  edit_gestures_.detach();
  data_gestures_.detach();
  scene_gestures_.detach();
  // Caller (Browser::prepare_close) must detach MapViewport / join Display
  // before this so abandon does not race a live present_mu_ holder.
  scene3d_.abandon_mesh();
  scene3d_stereo_.release();
}

void MapSession::set_map_contents_observer(MapContentsObserver* observer) {
  if (map_contents_) {
    map_contents_->SetObserver(observer);
  }
}

void MapSession::clear_map_contents_observer() {
  if (map_contents_) {
    map_contents_->SetObserver(nullptr);
  }
}

}  // namespace content
