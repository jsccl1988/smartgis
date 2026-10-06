// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_CONTENTS_H
#define CONTENT_PUBLIC_MAP_CONTENTS_H

#include <cstddef>
#include <cstdint>

#include "content/content_export.h"
#include "content/public/map_layer_types.h"

// Browser-process session: relaunch this PE with --type=gpu, N
// MapWidgetHostView surfaces. Hosts include only content/public.
namespace content {

// Shell implements this. MapContents does not include mojo.
class MapContentsObserver {
 public:
  virtual ~MapContentsObserver() = default;
  virtual void OnFrameReady(uint32_t view_id, uint32_t generation) {}
  virtual void OnExtentChanged(uint32_t view_id, const Extent2& e) {}
  virtual void OnRenderDied() {}
};

// Hosted map viewport. Shell presents Latest() into its HWND.
// GPU owns RenderDevice2d. Do not include sdb or render device headers.
class CONTENT_EXPORT MapWidgetHostView {
 public:
  struct CreateParams {
    void* parent_hwnd;
    CreateParams() : parent_hwnd(nullptr) {}
  };

  struct Preferences {};

  virtual ~MapWidgetHostView() = default;

  virtual void Create(const CreateParams& params,
                      const Preferences& preferences) = 0;
  virtual void Destroy() = 0;

  virtual uint32_t ViewId() const = 0;
  virtual void* NativeHwnd() const = 0;

  virtual void Resize(int width_px, int height_px, float dpi) = 0;
  virtual void Resize(int x,
                       int y,
                       int width_px,
                       int height_px,
                       float dpi) = 0;
  virtual void SetPresentMode(PresentMode mode) = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual SharedSurface Latest() const = 0;
};

class CONTENT_EXPORT MapContents {
 public:
  virtual ~MapContents() = default;

  static MapContents* Create();

  virtual bool StartRenderProcess() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsOopRender() const = 0;
  virtual const wchar_t* PresentStatus() const = 0;

  virtual uint32_t OpenView(ViewKind kind) = 0;
  virtual void CloseView(uint32_t view_id) = 0;
  virtual MapWidgetHostView* AttachSurface(uint32_t view_id,
                                           PresentMode mode) = 0;
  virtual MapWidgetHostView* HostView(uint32_t view_id) = 0;

  virtual void SetExtent(uint32_t view_id, const Extent2& e) = 0;
  virtual Extent2 Extent(uint32_t view_id) const = 0;

  virtual void SetSelection(uint32_t view_id,
                           const FeatureId* ids,
                           size_t n) = 0;
  virtual void LegendSnapshot(uint32_t view_id) = 0;
  virtual void CatalogCall(const char* json_op) = 0;

  virtual void DispatchPlugin(uint32_t view_id,
                              const char* plugin_id,
                              const char* method,
                              const void* bytes,
                              size_t n) = 0;

  virtual void ActivateTool(uint32_t view_id, const char* tool_id) = 0;
  virtual void Dispatch(uint32_t view_id, const InputEvent& e) = 0;

  // Tell --type=gpu which 2D paint path to use. 0 = Track B RHI, 1 = Track A
  // MapLibre. Hot-swap; both tracks paint into PresentTarget (no GL/DX restart).
  virtual void SetRenderBackend(uint32_t kind) = 0;
  virtual uint32_t RenderBackend() const = 0;

  virtual void SetObserver(MapContentsObserver* observer) = 0;
  virtual bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_CONTENTS_H
