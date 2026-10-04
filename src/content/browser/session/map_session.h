// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_SESSION_MAP_SESSION_H_
#define CONTENT_BROWSER_SESSION_MAP_SESSION_H_

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

// In-process map session (Chromium WebContents analogue for Views).
// Owns document / camera / present facades / HWND gestures / ViewHosts and
// the optional MapContents OOP/GPU pipe. Not absorbed into content.dll —
// linked via //src/content:map_session (see shell §Content sink C5/C6).
class MapSession {
 public:
  MapSession();
  ~MapSession();

  MapSession(const MapSession&) = delete;
  MapSession& operator=(const MapSession&) = delete;

  // Create ViewHosts + MapContents::Create. StartRenderProcess is deferred
  // until ensure_oop_render_process() unless SMT_ENABLE_OOP_RENDER=1.
  // OOP failure clears map_contents() only for the opt-in-at-init path.
  void init_hosts();

  // Lazily start the OOP GPU child. No-op when already running or when
  // SMT_DISABLE_OOP_RENDER=1. Returns true when IsOopRender().
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
  Map2dPresenter& map2d() { return map2d_; }
  const Map2dPresenter& map2d() const { return map2d_; }
  Scene3dPresenter& scene3d() { return scene3d_; }
  const Scene3dPresenter& scene3d() const { return scene3d_; }
  Scene3dStereoSession& scene3d_stereo() { return scene3d_stereo_; }
  BlitFrameCache& blit() { return blit_; }
  ViewNavigation& navigation() { return navigation_; }
  const ViewNavigation& navigation() const { return navigation_; }

  MapHwndGestures& edit_gestures() { return edit_gestures_; }
  MapHwndGestures& data_gestures() { return data_gestures_; }
  MapHwndGestures& scene_gestures() { return scene_gestures_; }

  // Non-const pointers from const MapSession match std::unique_ptr::get().
  MapContents* map_contents() const { return map_contents_.get(); }
  ViewHost* edit_host() const { return edit_host_.get(); }
  ViewHost* data_host() const { return data_host_.get(); }
  ViewHost* scene_host() const { return scene_host_.get(); }

 private:
  // Hosts / MapContents first: large presenters below have historically smashed
  // trailing unique_ptrs when a TU skews Scene3dPresenter / Map2dPresenter
  // sizeof (multi-agent partial rebuild) — wire_tool_seams then AVs on
  // 0xCDCDCD.. ViewHost*. Keeping owned pointers ahead of those blobs isolates
  // the edit/data/scene seams from present-path layout drift.
  std::unique_ptr<ViewHost> edit_host_;
  std::unique_ptr<ViewHost> data_host_;
  std::unique_ptr<ViewHost> scene_host_;
  std::unique_ptr<MapContents> map_contents_;
  bool prepare_close_done_ = false;

  MapScene document_;
  ViewFrame view_frame_;
  OrbitFrame orbit_;
  Map2dPresenter map2d_;
  Scene3dPresenter scene3d_;
  Scene3dStereoSession scene3d_stereo_;
  BlitFrameCache blit_;
  ViewNavigation navigation_;
  MapHwndGestures edit_gestures_;
  MapHwndGestures data_gestures_;
  MapHwndGestures scene_gestures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SESSION_MAP_SESSION_H_
