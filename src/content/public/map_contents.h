// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_CONTENTS_H
#define CONTENT_PUBLIC_MAP_CONTENTS_H

#include <cstddef>
#include <cstdint>

#include "content/public/map_contents_observer.h"
#include "content/public/map_types.h"
#include "content/public/map_widget_host_view.h"

// Browser-process session: relaunch this PE with --type=gpu, N
// MapWidgetHostView surfaces. Hosts include only content/public.
namespace content {

class MapContents {
 public:
  virtual ~MapContents() = default;

  static MapContents* Create();
  // When set, StartRenderProcess launches this PE instead of this process.
  // Used by C# / CEF hosts that cannot relaunch themselves as --type=gpu.
  // Empty / null clears the override.
  static void SetGpuExeOverride(const wchar_t* utf16_path);

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

  virtual void SetObserver(MapContentsObserver* observer) = 0;
  virtual bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_CONTENTS_H
