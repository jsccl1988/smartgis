// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Viewport pane bag the IL runtime sees. The shell fills each Op from
// DrawHost so glang.runtime does not include the Views viewport type.
// Same shape as DocumentCapability: traits-constrained detail::Op slots,
// no virtual pane ABI.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_PANE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_PANE_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/capability/op.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/kernel/view/view.h"

namespace content {

class GisContents;

// Shell-owned map / data / scene surface. Empty Ops are false / no-ops.
struct Pane {
  enum class AttachMode {
    kNone,
    kContentMapView,
    kOopRender,
    kGpuPresent,
    kLocalDevice,
    kPlaceholder,
  };

  enum class Role {
    kMapEdit,
    kMapData,
    kScene3d,
  };

  detail::Op<Role()> role;
  detail::Op<AttachMode()> attach_mode;
  detail::Op<bool()> is_visible;
  detail::Op<HWND()> native_view;
  detail::Op<void()> sync_native_bounds;
  detail::Op<void()> invalidate_native;
  detail::Op<void(bool)> set_gpu_present_visible;
  detail::Op<void()> resume_present_timer;
  detail::Op<void()> pause_present;
  detail::Op<HWND()> present_hwnd;
  detail::Op<HWND()> input_hwnd;
  detail::Op<HWND()> identity_hud_hwnd;
  detail::Op<void()> request_frame;
  detail::Op<bool()> attach;
  detail::Op<void()> detach;
  detail::Op<void()> realize_native;
  detail::Op<void()> sync_identity_frame;
  detail::Op<void*()> rhi_device;
  detail::Op<bool(std::uint32_t)> wait_ready;
  detail::Op<bool()> last_gpu_present_ok;
  detail::Op<std::uint32_t()> frame_presented;
  detail::Op<std::uint32_t()> view_id;
  detail::Op<GisContents*()> gis_contents;
  detail::Op<float()> hud_fps;
  detail::Op<ui::views::Rect()> bounds;
  detail::Op<ui::views::View*()> as_view;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_PANE_H_
