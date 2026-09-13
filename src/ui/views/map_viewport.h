// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_H_
#define UI_VIEWS_MAP_VIEWPORT_H_

#include <cstdint>

#include <windows.h>

#include "ui/views/view.h"

namespace content {
class MapContents;
}

namespace ui {
namespace views {

// Native-hosted map pane. Hang is mgis-like: parent HWND + child HWND.
// Pixels still come from existing gis + render, or the same PE with --type=gpu.
class MapViewport : public View {
 public:
  enum class AttachMode {
    kNone,
    kContentMapView,
    kOopRender,
    kLocalDevice,
    kPlaceholder,
  };

  MapViewport();
  ~MapViewport() override;

  // Prefer content::MapWidgetHostView (same PE --type=gpu), then leftover
  // SmartGisRender.exe, then LoadLibrary SmtRenderDevice::Init.
  bool attach();
  AttachMode attach_mode() const { return mode_; }
  const wchar_t* status_text() const { return status_; }
  bool wait_ready(uint32_t timeout_ms);

  void detach();

 protected:
  HWND create_native_view(HWND parent) override;
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  bool try_content_map_view();
  bool try_oop_render();
  bool try_local_device();
  void paint_child_placeholder();

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);

  AttachMode mode_ = AttachMode::kNone;
  const wchar_t* status_ = L"";
  content::MapContents* session_ = nullptr;
  HANDLE render_process_ = nullptr;
  HANDLE render_job_ = nullptr;
  void* local_device_ = nullptr;
  HMODULE local_module_ = nullptr;
  uint32_t view_id_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_H_
