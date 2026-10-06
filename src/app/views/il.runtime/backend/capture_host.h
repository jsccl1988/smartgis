// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_HOST_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_HOST_H_

#include <cstdint>
#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace ui::views {
class DrawHost;
}  // namespace ui::views

namespace app {

class Browser;

namespace detail {

// Viewport teardown: stop present timers then detach all DrawHost panes.
// Does not abandon_mesh (unsafe under FlyCube Scene3D teardown).
void detach_maps(Browser& browser);

// Stops DrawHost present timers (timer id 1) without full detach.
void stop_map_present_timers(Browser& browser);

// Restarts present timers + one frame request (browse BMP after stress).
void resume_map_present_timers(Browser& browser);

// Invalidate + UpdateWindow + sync_identity_frame. One paint kick.
void kick_draw_host_paint(ui::views::DrawHost* pane);

// Kick the shell Display mailbox and wait until presented advances.
// Does not call Device methods on the UI thread.
bool present_shell_scene3d_frame(ui::views::DrawHost* scene,
                                 DWORD wait_ms = 2000);

// Single UI-thread present_gpu. False when cam/device is null or fail.
bool present_scene3d_gpu(content::Scene3dPresenter* cam,
                         render::rhi::Device* device,
                         uint32_t width_px,
                         uint32_t height_px);

// FlyCube DXGI popup over the 3D pane, else input HWND, else embed native.
HWND shell_scene3d_capture_hwnd(ui::views::DrawHost* scene);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_HOST_H_
