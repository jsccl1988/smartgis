// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/cef_map_slot.h"

#include "app/views/map_host_extent.h"
#include "app/views/scene3d_rhi_session.h"
#include "tool/camera_nav.h"
#include "content/public/map_contents.h"
#include "content/public/map_widget_host_view.h"
#include "content/public/view_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace app {
namespace cef {
namespace {

constexpr wchar_t kChildClass[] = L"SmartGisCefMapSlot";

void register_child_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = CefMapSlot::wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kChildClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

CefMapSlot::CefMapSlot() = default;

CefMapSlot::~CefMapSlot() {
  destroy();
}

bool CefMapSlot::create(HWND parent,
                        content::MapContents* session,
                        content::ViewKind kind) {
  destroy();
  parent_ = parent;
  session_ = session;
  kind_ = kind;
  if (!parent_ || !session_) {
    return false;
  }

  // StartRenderProcess is owned by BrowserMain (once per session).
  render_ok_ = true;
  view_host_ = new content::ViewHost();

  register_child_class();
  child_hwnd_ = CreateWindowExW(
      0, kChildClass, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, parent_,
      nullptr, GetModuleHandleW(nullptr), this);
  if (!child_hwnd_) {
    return false;
  }
  GESTURECONFIG gc = {};
  gc.dwID = GID_ZOOM;
  gc.dwWant = GC_ZOOM;
  SetGestureConfig(child_hwnd_, 0, 1, &gc, sizeof(gc));

  // Scene3d: FlyCube first when preferred; stereo/GDI fallback on attach fail.
  // Always OpenView so self-test wait_frame remains available (WinUI parity).
  bool flycube_live = false;
  if (kind_ == content::ViewKind::kScene3d && prefer_scene3d_flycube() &&
      scene3d_rhi_.try_attach(child_hwnd_)) {
    flycube_live = true;
  } else {
    scene3d_rhi_.release();
  }
  view_id_ = session_->OpenView(kind_);
  view_ = session_->AttachSurface(view_id_, content::PresentMode::kSoftwareDib);
  if (view_) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = child_hwnd_;
    view_->Create(params, content::MapWidgetHostView::Preferences());
    view_->SetPresentMode(content::PresentMode::kSoftwareDib);
  }
  // Do not create the GL stereo device on the initial 1x1 child. Attach once
  // sync_layout has a real client (see size_changed branch).
  if (flycube_live) {
    scene3d_stereo_.release();
  }
  bind_scene3d();
  start_present_timer();
  set_visible(false);
  return true;
}

void CefMapSlot::set_document(MapScene* document) {
  document_ = document;
  bind_scene3d();
}

MapScene* CefMapSlot::dem_map_scene() {
  return document_ ? document_ : &map_scene_;
}

const MapScene* CefMapSlot::dem_map_scene() const {
  return document_ ? document_ : &map_scene_;
}

void CefMapSlot::bind_scene3d() {
  MapScene* scene = dem_map_scene();
  if (scene->feature_count() == 0) {
    // Owned fallback only — do not mutate a shared chrome document here.
    if (scene == &map_scene_) {
      map_scene_.seed_default();
    }
  }
  scene3d_.bind_map(scene);
  content::Extent2 e = scene->world_extent();
  if (!app::extent_looks_like_china(e)) {
    e = app::kChinaLonLatExtent;
  }
  if (session_ && view_id_ != 0) {
    session_->SetExtent(view_id_, e);
  }
  scene3d_.bind_contents(session_, view_id_);
  scene3d_.apply_world_extent(e);
}

void CefMapSlot::destroy() {
  stop_present_timer();
  scene3d_stereo_.release();
  scene3d_rhi_.release();
  if (child_hwnd_) {
    DestroyWindow(child_hwnd_);
    child_hwnd_ = nullptr;
  }
  if (session_ && view_id_ != 0) {
    session_->CloseView(view_id_);
    view_id_ = 0;
  }
  view_ = nullptr;
  delete view_host_;
  view_host_ = nullptr;
  session_ = nullptr;
  parent_ = nullptr;
  document_ = nullptr;
  view_menu_requested_ = nullptr;
}

void CefMapSlot::sync_layout(const RectPx& rect_px, float dpi) {
  if (!child_hwnd_ || !rect_px.is_valid()) {
    return;
  }
  // Skip no-op SetWindowPos: repeated HWND_TOP + Resize recreates the GPU
  // DIB while present_latest_frame is still mapping it (AV / flicker).
  RECT wr = {};
  GetWindowRect(child_hwnd_, &wr);
  POINT tl = {wr.left, wr.top};
  if (HWND parent = GetParent(child_hwnd_)) {
    ScreenToClient(parent, &tl);
  }
  const int cur_w = wr.right - wr.left;
  const int cur_h = wr.bottom - wr.top;
  const bool shown = IsWindowVisible(child_hwnd_) != FALSE;
  const bool same_pos =
      tl.x == rect_px.x && tl.y == rect_px.y && cur_w == rect_px.w &&
      cur_h == rect_px.h && (visible_ ? shown : !shown);
  const bool size_changed = last_layout_w_ != rect_px.w ||
                            last_layout_h_ != rect_px.h || cur_w != rect_px.w ||
                            cur_h != rect_px.h;
  const bool dpi_changed = last_dpi_ != 0.f && last_dpi_ != dpi;
  if (!same_pos) {
    SetWindowPos(child_hwnd_, HWND_TOP, rect_px.x, rect_px.y, rect_px.w,
                 rect_px.h,
                 SWP_NOACTIVATE | (visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  last_layout_x_ = rect_px.x;
  last_layout_y_ = rect_px.y;
  last_layout_w_ = rect_px.w;
  last_layout_h_ = rect_px.h;
  last_dpi_ = dpi;
  if (view_ && (size_changed || dpi_changed || !has_presented_frame())) {
    view_->Resize(rect_px.w, rect_px.h, dpi * 96.f);
  }
  if (size_changed) {
    MapScene* scene = dem_map_scene();
    if (scene->feature_count() > 0) {
      scene->fit_extent(rect_px.w, rect_px.h);
      if (kind_ == content::ViewKind::kScene3d) {
        scene3d_.apply_world_extent(scene->world_extent());
      }
      InvalidateRect(child_hwnd_, nullptr, FALSE);
    }
    if (scene3d_rhi_.is_live() && rect_px.w > 0 && rect_px.h > 0) {
      scene3d_rhi_.resize(child_hwnd_, static_cast<uint32_t>(rect_px.w),
                          static_cast<uint32_t>(rect_px.h));
    }
    if (kind_ == content::ViewKind::kScene3d && rect_px.w > 8 &&
        rect_px.h > 8 && !prefer_scene3d_flycube()) {
      if (!scene3d_stereo_.is_live()) {
        (void)scene3d_stereo_.try_attach(child_hwnd_);
      }
      scene3d_stereo_.resize(rect_px.w, rect_px.h);
    }
  }
}

void CefMapSlot::set_visible(bool visible) {
  visible_ = visible;
  if (!child_hwnd_) {
    return;
  }
  ShowWindow(child_hwnd_, visible ? SW_SHOW : SW_HIDE);
  if (view_) {
    view_->SetVisible(visible);
  }
}

bool CefMapSlot::wait_ready(uint32_t timeout_ms) {
  if (!session_ || view_id_ == 0 || !render_ok_) {
    return false;
  }
  if (!session_->WaitFrameReady(view_id_, timeout_ms)) {
    return false;
  }
  // Require a presentable DIB (not merely a generation bump).
  const DWORD end = GetTickCount() + 2000;
  for (;;) {
    if (has_presented_frame()) {
      return true;
    }
    if (GetTickCount() >= end) {
      return false;
    }
    Sleep(20);
  }
}

bool CefMapSlot::has_presented_frame() const {
  if (!view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px > 0 && surface.height_px > 0;
}

uint32_t CefMapSlot::presented_generation() const {
  if (!view_) {
    return 0;
  }
  return view_->Latest().generation;
}

content::ViewHost* CefMapSlot::view_host() {
  return view_host_;
}

void CefMapSlot::start_present_timer() {
  if (child_hwnd_) {
    SetTimer(child_hwnd_, kPresentTimerId, 33, nullptr);
  }
}

void CefMapSlot::stop_present_timer() {
  if (child_hwnd_) {
    KillTimer(child_hwnd_, kPresentTimerId);
  }
}

bool CefMapSlot::present_latest_frame(HDC hdc, const RECT& client_rc) {
  if (!hdc || !view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  if (!surface.nt_handle || surface.generation == 0 || surface.width_px == 0 ||
      surface.height_px == 0) {
    return false;
  }
  if (surface.width_px > 8192u || surface.height_px > 8192u) {
    return false;
  }
  const SIZE_T bytes = static_cast<SIZE_T>(surface.width_px) *
                       static_cast<SIZE_T>(surface.height_px) * 4u;
  void* bits =
      MapViewOfFile(static_cast<HANDLE>(surface.nt_handle), FILE_MAP_READ, 0, 0,
                    bytes);
  if (!bits) {
    bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                         FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  }
  if (!bits) {
    return false;
  }
  MEMORY_BASIC_INFORMATION mbi = {};
  SIZE_T mapped = 0;
  if (VirtualQuery(bits, &mbi, sizeof(mbi)) != 0) {
    mapped = mbi.RegionSize;
  }
  if (mapped != 0 && mapped < bytes) {
    UnmapViewOfFile(bits);
    return false;
  }
  // Copy out of the shared mapping before StretchDIBits so a concurrent GPU
  // resize cannot invalidate the source mid-blit (Views / WinUI parity).
  std::vector<uint8_t> local(bytes);
  std::memcpy(local.data(), bits, bytes);
  UnmapViewOfFile(bits);

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = static_cast<LONG>(surface.width_px);
  bi.bmiHeader.biHeight = -static_cast<LONG>(surface.height_px);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  const int dst_w = client_rc.right > 0 ? client_rc.right : 1;
  const int dst_h = client_rc.bottom > 0 ? client_rc.bottom : 1;
  const int ok =
      StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0,
                    static_cast<int>(surface.width_px),
                    static_cast<int>(surface.height_px), local.data(), &bi,
                    DIB_RGB_COLORS, SRCCOPY);
  if (ok == 0 || ok == GDI_ERROR) {
    return false;
  }
  painted_generation_ = surface.generation;
  return true;
}

void CefMapSlot::client_size(int* w, int* h) const {
  int cw = last_layout_w_;
  int ch = last_layout_h_;
  if (child_hwnd_) {
    RECT rc = {};
    GetClientRect(child_hwnd_, &rc);
    cw = rc.right - rc.left;
    ch = rc.bottom - rc.top;
  }
  if (w) {
    *w = cw;
  }
  if (h) {
    *h = ch;
  }
}

void CefMapSlot::schedule_full_redraw() {
  if (!child_hwnd_) {
    return;
  }
  KillTimer(child_hwnd_, kBlitTimerId);
  SetTimer(child_hwnd_, kBlitTimerId,
           static_cast<UINT>(tool::kBlitDebounceMs), nullptr);
}

void CefMapSlot::commit_blit_preview() {
  blit_.end_preview();
  invalidate();
}

void CefMapSlot::preview_zoom_at(int x_px, int y_px, double factor) {
  int w = 0;
  int h = 0;
  client_size(&w, &h);
  if (kind_ == content::ViewKind::kScene3d) {
    if (w >= 8 && h >= 8) {
      const int32_t wheel = factor > 1.0 ? WHEEL_DELTA : -WHEEL_DELTA;
      scene3d_.apply_wheel_at(x_px, y_px, wheel, w, h);
    }
    invalidate();
    schedule_full_redraw();
    return;
  }
  if (w >= 8 && h >= 8) {
    blit_.begin_zoom(w, h, x_px, y_px, factor);
  }
  if (document_) {
    document_->apply_zoom_at(x_px, y_px, factor);
  }
  invalidate();
  schedule_full_redraw();
}

void CefMapSlot::preview_pan(int dx_px, int dy_px) {
  if (kind_ == content::ViewKind::kScene3d) {
    scene3d_.apply_pan(dx_px, dy_px);
    invalidate();
    schedule_full_redraw();
    return;
  }
  int w = 0;
  int h = 0;
  client_size(&w, &h);
  if (w >= 8 && h >= 8) {
    blit_.begin_pan(w, h, dx_px, dy_px);
  }
  if (document_) {
    document_->apply_pan(dx_px, dy_px);
  }
  invalidate();
  schedule_full_redraw();
}

void CefMapSlot::apply_scene3d_draft(const tool::Draft& draft) {
  if (kind_ != content::ViewKind::kScene3d) {
    return;
  }
  scene3d_.apply_draft(draft);
  invalidate();
  schedule_full_redraw();
}

void CefMapSlot::paint_to_dc(HDC hdc, const RECT& rc) {
  if (!hdc) {
    return;
  }
  const int w = rc.right > 0 ? rc.right : 1;
  const int h = rc.bottom > 0 ? rc.bottom : 1;
  // Scene3d: FlyCube → leftover GL stereo → GDI DEM.
  // ContentMapView DIB is a static GPU demo — do not leave it as the frame.
  if (kind_ == content::ViewKind::kScene3d) {
    if (scene3d_rhi_.is_live() && prefer_scene3d_flycube() && w > 0 && h > 0) {
      const bool ok = scene3d_rhi_.present(
          const_cast<Scene3dController*>(&scene3d_),
          static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      if (ok) {
        scene3d_.paint_hud(hdc, w, h);
        return;
      }
    }
    // Stereo SwapBuffers targets the child HWND. A memory DC is later BitBlt
    // over that window and would hide the GL front buffer, so only GDI-fallback
    // into it. Window DCs (BeginPaint) present in place.
    if (w > 0 && h > 0 && GetObjectType(hdc) != OBJ_MEMDC) {
      if (scene3d_stereo_.try_present_sot(child_hwnd_, hdc, w, h,
                                          scene3d_.yaw(), scene3d_.pitch(),
                                          scene3d_.distance())) {
        return;
      }
      scene3d_.paint(hdc, w, h, /*fill_background=*/true);
    } else if (w > 0 && h > 0) {
      scene3d_.paint(hdc, w, h, /*fill_background=*/true);
    }
    return;
  }
  if (blit_.in_preview() && blit_.present(hdc, w, h)) {
    return;
  }
  // Product 2D is MapScene (scale tiers, collision, river gates). A GPU DIB
  // left underneath keeps the old cartography visible wherever the scene
  // does not fill.
  MapScene* overlay = dem_map_scene();
  const int paint_w = rc.right - rc.left;
  const int paint_h = rc.bottom - rc.top;
  const bool map_owns_frame =
      overlay && overlay->feature_count() > 0 && paint_w > 0 && paint_h > 0;
  bool presented = false;
  if (!map_owns_frame) {
    presented = present_latest_frame(hdc, rc);
    if (!presented) {
      const HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
      FillRect(hdc, &rc, brush);
      DeleteObject(brush);
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(60, 70, 80));
      const wchar_t* line = render_ok_ ? L"Map slot (waiting for frame)"
                                       : L"Map slot (GPU not started)";
      DrawTextW(hdc, line, -1, const_cast<RECT*>(&rc),
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
  }
  if (map_owns_frame) {
    overlay->paint(hdc, paint_w, paint_h, /*fill_background=*/true);
  }
  if (paint_w > 0 && paint_h > 0) {
    blit_.capture(hdc, paint_w, paint_h);
  }
}

void CefMapSlot::paint_child() {
  if (!child_hwnd_) {
    return;
  }
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(child_hwnd_, &ps);
  RECT rc;
  GetClientRect(child_hwnd_, &rc);
  const int w = rc.right;
  const int h = rc.bottom;
  // Leftover GL SwapBuffers on this HWND. An offscreen DIB BitBlt does not
  // capture the GL front buffer and then covers it.
  if (kind_ == content::ViewKind::kScene3d && w > 0 && h > 0 &&
      !(scene3d_rhi_.is_live() && prefer_scene3d_flycube())) {
    if (scene3d_stereo_.try_present_sot(child_hwnd_, hdc, w, h, scene3d_.yaw(),
                                        scene3d_.pitch(),
                                        scene3d_.distance())) {
      EndPaint(child_hwnd_, &ps);
      return;
    }
  }
  // Match MapViewport Scene3d: FlyCube presents to HWND swapchain (HUD only).
  if (kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live() &&
      prefer_scene3d_flycube() && w > 0 && h > 0) {
    paint_to_dc(hdc, rc);
    EndPaint(child_hwnd_, &ps);
    return;
  }
  if (w > 0 && h > 0) {
    HDC mem = CreateCompatibleDC(hdc);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp =
        CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (mem && bmp) {
      HGDIOBJ old = SelectObject(mem, bmp);
      paint_to_dc(mem, rc);
      BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
      SelectObject(mem, old);
      DeleteObject(bmp);
    } else if (hdc) {
      paint_to_dc(hdc, rc);
    }
    if (mem) {
      DeleteDC(mem);
    }
  }
  EndPaint(child_hwnd_, &ps);
}

void CefMapSlot::show_view_menu(POINT screen_pt) const {
  if (view_menu_requested_) {
    view_menu_requested_(screen_pt);
  }
}

void CefMapSlot::dispatch_mouse(content::InputEvent::Kind kind,
                                LPARAM lparam,
                                int wheel) {
  if (!view_host_ || !visible_) {
    return;
  }
  content::InputEvent e{};
  e.kind = kind;
  e.x_px = static_cast<int16_t>(LOWORD(lparam));
  e.y_px = static_cast<int16_t>(HIWORD(lparam));
  e.wheel = wheel;
  view_host_->dispatch_input(e);
  if (session_ && view_id_ != 0) {
    session_->Dispatch(view_id_, e);
  }
}

void CefMapSlot::dispatch_pinch_zoom(int x_px, int y_px, double scale) {
  if (!visible_ || scale <= 0.0) {
    return;
  }
  if (kind_ == content::ViewKind::kScene3d) {
    int w = 0;
    int h = 0;
    client_size(&w, &h);
    if (w >= 8 && h >= 8) {
      scene3d_.apply_pinch(x_px, y_px, scale, w, h);
      invalidate();
      schedule_full_redraw();
    }
    return;
  }
  const int wheel = scale > 1.0 ? WHEEL_DELTA : (scale < 1.0 ? -WHEEL_DELTA : 0);
  if (wheel == 0) {
    return;
  }
  content::InputEvent e{};
  e.kind = content::InputEvent::Kind::kWheel;
  e.x_px = x_px;
  e.y_px = y_px;
  e.wheel = wheel;
  if (view_host_) {
    view_host_->dispatch_input(e);
  }
  if (session_ && view_id_ != 0) {
    session_->Dispatch(view_id_, e);
  }
  preview_zoom_at(x_px, y_px, scale > 1.0 ? 1.25 : 0.8);
}

bool CefMapSlot::handle_gesture(WPARAM, LPARAM lparam) {
  GESTUREINFO gi = {};
  gi.cbSize = sizeof(gi);
  if (!GetGestureInfo(reinterpret_cast<HGESTUREINFO>(lparam), &gi)) {
    return false;
  }
  const bool handled = gi.dwID == GID_ZOOM || gi.dwID == GID_BEGIN ||
                       gi.dwID == GID_END;
  if (gi.dwID == GID_BEGIN) {
    last_zoom_distance_ = gi.ullArguments;
    zoom_gesture_active_ = true;
  } else if (gi.dwID == GID_END) {
    zoom_gesture_active_ = false;
    last_zoom_distance_ = 0;
  } else if (gi.dwID == GID_ZOOM && zoom_gesture_active_ &&
             last_zoom_distance_ > 0 && gi.ullArguments > 0) {
    const double scale = static_cast<double>(gi.ullArguments) /
                         static_cast<double>(last_zoom_distance_);
    last_zoom_distance_ = gi.ullArguments;
    POINT pt = {gi.ptsLocation.x, gi.ptsLocation.y};
    ScreenToClient(child_hwnd_, &pt);
    dispatch_pinch_zoom(pt.x, pt.y, scale);
  }
  CloseGestureInfoHandle(reinterpret_cast<HGESTUREINFO>(lparam));
  return handled;
}

LRESULT CALLBACK CefMapSlot::wnd_proc(HWND hwnd,
                                      UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  CefMapSlot* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<CefMapSlot*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<CefMapSlot*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case WM_PAINT:
      if (self) {
        self->paint_child();
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER:
      if (self && wparam == kBlitTimerId) {
        KillTimer(hwnd, kBlitTimerId);
        self->commit_blit_preview();
        return 0;
      }
      if (self && wparam == kPresentTimerId) {
        if (self->kind_ == content::ViewKind::kScene3d &&
            IsWindowVisible(hwnd)) {
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (self->view_) {
          const content::SharedSurface surface = self->view_->Latest();
          if (surface.generation != 0 &&
              surface.generation != self->painted_generation_) {
            InvalidateRect(hwnd, nullptr, FALSE);
          }
        }
      }
      return 0;
    case WM_LBUTTONDOWN:
      if (self) {
        SetCapture(hwnd);
        self->dispatch_mouse(content::InputEvent::Kind::kLDown, lparam, 0);
      }
      return 0;
    case WM_LBUTTONUP:
      if (self) {
        ReleaseCapture();
        self->dispatch_mouse(content::InputEvent::Kind::kLUp, lparam, 0);
      }
      return 0;
    case WM_LBUTTONDBLCLK:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kLDClick, lparam, 0);
      }
      return 0;
    case WM_RBUTTONDOWN:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kRDown, lparam, 0);
      }
      return 0;
    case WM_RBUTTONUP:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kRUp, lparam, 0);
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ClientToScreen(hwnd, &pt);
        self->show_view_menu(pt);
      }
      return 0;
    case WM_CONTEXTMENU:
      if (self) {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (pt.x == -1 && pt.y == -1) {
          RECT rc = {};
          GetClientRect(hwnd, &rc);
          pt.x = rc.left + (rc.right - rc.left) / 2;
          pt.y = rc.top + (rc.bottom - rc.top) / 2;
          ClientToScreen(hwnd, &pt);
        }
        self->show_view_menu(pt);
      }
      return 0;
    case WM_MOUSEMOVE:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kMouseMove, lparam, 0);
      }
      return 0;
    case WM_GESTURE:
      if (self && self->handle_gesture(wparam, lparam)) {
        return 0;
      }
      break;
    case WM_MOUSEWHEEL:
      if (self) {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        const LPARAM client_lp = MAKELPARAM(pt.x, pt.y);
        self->dispatch_mouse(content::InputEvent::Kind::kWheel, client_lp,
                             GET_WHEEL_DELTA_WPARAM(wparam));
        const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
        self->preview_zoom_at(pt.x, pt.y, delta > 0 ? 1.25 : 0.8);
      }
      return 0;
    case WM_KEYDOWN:
      if (self && self->view_host_) {
        content::InputEvent e{};
        e.kind = content::InputEvent::Kind::kKeyDown;
        e.key = static_cast<uint32_t>(wparam);
        self->view_host_->dispatch_input(e);
      }
      return 0;
    case WM_NCDESTROY:
      if (self) {
        self->child_hwnd_ = nullptr;
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace cef
}  // namespace app
