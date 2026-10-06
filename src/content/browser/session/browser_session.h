// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_
#define CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_

#include <memory>

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/camera/view_navigation.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/input/map_hwnd_gestures.h"
#include "content/browser/present/host/blit_frame_cache.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_stereo_session.h"

namespace content {

class MapContents;
class MapContentsObserver;
class ViewHost;

// In-process browser session (Chromium WebContents analogue for Views).
// Owns the document, camera, map2d and scene3d present facades, HWND
// gestures, ViewHosts, and the optional MapContents OOP/GPU pipe.
// Not absorbed into content.dll — linked via //src/content:browser_session
// (see shell §Content sink C5/C6).
class BrowserSession {
 public:
  // Heap-allocate in this TU. Embedding BrowserSession by value in app::Browser
  // used browser.cc sizeof vs this TU's member ctors and smashed a freed CRT
  // block (HEAP: modified after free → string member in Browser()).
  static std::unique_ptr<BrowserSession> create();

  BrowserSession();
  ~BrowserSession();

  BrowserSession(const BrowserSession&) = delete;
  BrowserSession& operator=(const BrowserSession&) = delete;

  // Create ViewHosts. MapContents::Create is deferred to
  // ensure_oop_render_process() (eager Create during init heap-corrupted the
  // next CRT alloc in PluginShell::CommandCatalog). StartRenderProcess still
  // waits for ensure unless ENABLE_OOP_RENDER=1.
  void init_hosts();

  // Lazily start the OOP GPU child. No-op when already running or when
  // DISABLE_OOP_RENDER=1. Returns true when IsOopRender().
  bool ensure_oop_render_process();

  // Detach gestures / abandon mesh / release stereo before HWND teardown.
  void prepare_close();

  void set_map_contents_observer(MapContentsObserver* observer);
  void clear_map_contents_observer();

  MapScene& document() { return document_; }
  const MapScene& document() const { return document_; }
  ViewFrame& view_frame() { return view_frame_; }
  const ViewFrame& view_frame() const { return view_frame_; }
  OrbitFrame& orbit_frame() { return orbit_; }
  const OrbitFrame& orbit_frame() const { return orbit_; }
  Map2dPresenter& map2d() {
    if (!map2d_) {
      map2d_ = Map2dPresenter::create();
    }
    return *map2d_;
  }
  const Map2dPresenter& map2d() const { return const_cast<BrowserSession*>(this)->map2d(); }
  Scene3dPresenter& scene3d() {
    if (!scene3d_) {
      scene3d_ = Scene3dPresenter::create();
    }
    return *scene3d_;
  }
  const Scene3dPresenter& scene3d() const {
    return const_cast<BrowserSession*>(this)->scene3d();
  }
  Scene3dStereoSession& scene3d_stereo() { return scene3d_stereo_; }
  BlitFrameCache& blit() { return blit_; }
  ViewNavigation& navigation() { return navigation_; }
  const ViewNavigation& navigation() const { return navigation_; }

  MapHwndGestures& edit_gestures() { return edit_gestures_; }
  MapHwndGestures& data_gestures() { return data_gestures_; }
  MapHwndGestures& scene_gestures() { return scene_gestures_; }

  // Non-const pointers from const BrowserSession match std::unique_ptr::get().
  MapContents* map_contents() const { return map_contents_.get(); }
  ViewHost* edit_host() const { return edit_host_.get(); }
  ViewHost* data_host() const { return data_host_.get(); }
  ViewHost* scene_host() const { return scene_host_.get(); }

 private:
  // Hosts / MapContents first. Map2d/Scene3d presenters are heap Ptrs created
  // in their own TUs so a stale sizeof cannot overflow this object into the
  // CRT heap (0xC0000374 during ViewHost / Workspace::register_builtins).
  std::unique_ptr<ViewHost> edit_host_;
  std::unique_ptr<ViewHost> data_host_;
  std::unique_ptr<ViewHost> scene_host_;
  std::unique_ptr<MapContents> map_contents_;
  bool prepare_close_done_ = false;

  MapScene document_;
  ViewFrame view_frame_;
  OrbitFrame orbit_;
  Map2dPresenter::Ptr map2d_;
  Scene3dPresenter::Ptr scene3d_;
  Scene3dStereoSession scene3d_stereo_;
  BlitFrameCache blit_;
  ViewNavigation navigation_;
  MapHwndGestures edit_gestures_;
  MapHwndGestures data_gestures_;
  MapHwndGestures scene_gestures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_
