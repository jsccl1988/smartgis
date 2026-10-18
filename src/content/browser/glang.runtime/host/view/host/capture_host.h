// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_HOST_CAPTURE_HOST_H_
#define IL_RUNTIME_BACKEND_VIEW_HOST_CAPTURE_HOST_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include <cstdint>
#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace content {


namespace detail {

// While a live Python script is running, detach_views only stops present
// timers. DrawHost panes stay attached so export and the agent keep the shell.
void set_keep_live_views(bool on);
bool keep_live_views();

// Viewport teardown: stop present timers then detach all DrawHost panes.
// Does not abandon_mesh (unsafe under FlyCube Scene3D teardown).
void detach_views(content::ShellBridge& browser);

// Borrowed shell FlyCube: KillTimer + pause_present only. Full detach of a
// live Display-thread Device races the next process (0xC000041D).
void finish_scene3d(content::ShellBridge& browser, bool borrowed_shell);

// Stops DrawHost present timers (timer id 1) without full detach.
void stop_present_timers(content::ShellBridge& browser);

// Restarts present timers + one frame request (browse BMP after stress).
void resume_present_timers(content::ShellBridge& browser);

// Invalidate + UpdateWindow + sync_identity_frame. One paint kick.
void kick_draw_host_paint(content::Pane* pane);

// Kick the shell Display mailbox and wait until presented advances.
// Does not call Device methods on the UI thread.
bool present_shell_scene3d_frame(content::Pane* scene,
                                 DWORD wait_ms = 2000);

// Single UI-thread present_gpu. False when cam/device is null or fail.
bool present_scene3d_gpu(content::Scene3dPresenter* cam,
                         render::rhi::Device* device,
                         uint32_t width_px,
                         uint32_t height_px);

// FlyCube DXGI popup over the 3D pane, else input HWND, else embed native.
HWND shell_scene3d_capture_hwnd(content::Pane* scene);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_HOST_CAPTURE_HOST_H_
