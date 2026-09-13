// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_H_
#define UI_VIEWS_MAP_VIEWPORT_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/view.h"

namespace content {
class MapContents;
class ViewHost;
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

  // Which MapContents::OpenView kind this pane should request.
  enum class Role {
    kMapEdit,
    kMapData,
    kScene3d,
  };

  MapViewport();
  ~MapViewport() override;

  void set_role(Role role);
  Role role() const { return role_; }

  void set_view_host(content::ViewHost* host);
  content::ViewHost* view_host() const { return view_host_; }

  // Non-owning shared session. When unset, attach() may create one.
  void set_map_contents(content::MapContents* session);
  content::MapContents* map_contents() const { return session_; }

  uint32_t view_id() const { return view_id_; }

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
  void resize_host_surface(int width_px, int height_px);

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);

  AttachMode mode_ = AttachMode::kNone;
  Role role_ = Role::kMapEdit;
  const wchar_t* status_ = L"";
  content::MapContents* session_ = nullptr;
  content::ViewHost* view_host_ = nullptr;
  bool owns_session_ = false;
  HANDLE render_process_ = nullptr;
  HANDLE render_job_ = nullptr;
  void* local_device_ = nullptr;
  HMODULE local_module_ = nullptr;
  uint32_t view_id_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_H_
