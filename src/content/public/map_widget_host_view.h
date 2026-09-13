// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_WIDGET_HOST_VIEW_H
#define CONTENT_PUBLIC_MAP_WIDGET_HOST_VIEW_H

#include <cstdint>

#include "content/public/map_types.h"

// Hosted map viewport. Chrome presents Latest() into its HWND.
// GPU owns SmtRenderDevice. Do not include sdb or render device headers.
namespace content {

class MapWidgetHostView {
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

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_WIDGET_HOST_VIEW_H
