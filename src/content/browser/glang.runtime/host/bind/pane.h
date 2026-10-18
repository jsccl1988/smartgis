// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Viewport pane seen by the IL runtime. The shell adapts DrawHost to this
// surface so glang.runtime does not include the Views viewport type.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_BIND_PANE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_BIND_PANE_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/geometry/rect.h"
#include "ui/views/kernel/view/view.h"

namespace content {

class GisContents;

// Shell-owned map / data / scene surface. Methods match the viewport
// operations the runtime already calls; the shell supplies the body.
class Pane {
 public:
  virtual ~Pane() = default;

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

  virtual Role role() const = 0;
  virtual AttachMode attach_mode() const = 0;
  virtual bool is_visible() const = 0;
  virtual HWND native_view() const = 0;
  virtual void sync_native_bounds() = 0;
  virtual void invalidate_native() = 0;
  virtual void set_gpu_present_visible(bool show) = 0;
  virtual void resume_present_timer() = 0;
  virtual void pause_present() = 0;
  virtual HWND present_hwnd() const = 0;
  virtual HWND input_hwnd() const = 0;
  virtual HWND identity_hud_hwnd() const = 0;
  virtual void request_frame() = 0;
  virtual bool attach() = 0;
  virtual void detach() = 0;
  virtual void realize_native() = 0;
  virtual void sync_identity_frame() = 0;
  virtual void* rhi_device() const = 0;
  virtual bool wait_ready(std::uint32_t timeout_ms) = 0;
  virtual bool last_gpu_present_ok() const = 0;
  virtual std::uint32_t frame_presented() const = 0;
  virtual std::uint32_t view_id() const = 0;
  virtual GisContents* gis_contents() const = 0;
  virtual float hud_fps() const = 0;
  virtual ui::views::Rect bounds() const = 0;
  virtual ui::views::View* as_view() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_BIND_PANE_H_
