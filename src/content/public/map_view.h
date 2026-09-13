// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_VIEW_H
#define CONTENT_PUBLIC_MAP_VIEW_H

#include <cstdint>

#include "content/public/map_types.h"

// Hosted map viewport (mgis content::MapView). The app host presents
// latest() into its HWND. gpu owns SmtRenderDevice. Stable ABI only —
// do not include sdb or render device headers from app/ui.
namespace content {

class MapView {
 public:
  struct CreateParams {
    void* parent_hwnd;  // HWND in the chrome process; may be null
    CreateParams() : parent_hwnd(nullptr) {}
  };

  struct Preferences {};

  virtual ~MapView() = default;

  virtual void Create(const CreateParams& params,
                      const Preferences& preferences) = 0;
  virtual void Destroy() = 0;

  virtual uint32_t view_id() const = 0;
  virtual void* native_hwnd() const = 0;

  virtual void resize(int width_px, int height_px, float dpi) = 0;
  virtual void resize(int x,
                      int y,
                      int width_px,
                      int height_px,
                      float dpi) = 0;
  virtual void set_present_mode(PresentMode mode) = 0;
  virtual void set_visible(bool visible) = 0;
  virtual SharedSurface latest() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_VIEW_H
