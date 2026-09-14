// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/map_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>

namespace app {
namespace winui {
namespace {

constexpr wchar_t kChildClass[] = L"SmartGisWinuiMapHost";

void register_child_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MapHost::child_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kChildClass;
  RegisterClassExW(&wc);
  done = true;
}

bool class_is_xaml_island(const wchar_t* cls) {
  if (!cls || !cls[0]) {
    return false;
  }
  return wcsstr(cls, L"DesktopChildSiteBridge") != nullptr ||
         wcsstr(cls, L"DesktopWindowContentBridge") != nullptr ||
         wcsstr(cls, L"Windows.UI.Core.CoreWindow") != nullptr;
}

struct EnumIslandCtx {
  HWND found = nullptr;
};

BOOL CALLBACK enum_island_proc(HWND hwnd, LPARAM lp) {
  auto* ctx = reinterpret_cast<EnumIslandCtx*>(lp);
  wchar_t cls[256] = {};
  GetClassNameW(hwnd, cls, 256);
  if (class_is_xaml_island(cls)) {
    ctx->found = hwnd;
    return FALSE;
  }
  EnumChildWindows(hwnd, enum_island_proc, lp);
  return ctx->found == nullptr;
}

}  // namespace

int MapHost::slot_index(content::ViewKind kind) {
  if (kind == content::ViewKind::kMapData) {
    return 1;
  }
  if (kind == content::ViewKind::kScene3d) {
    return 2;
  }
  return 0;
}

MapHost::MapHost() {
  root_ = winrt::Microsoft::UI::Xaml::Controls::Grid();
  panel_ = winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel();
  status_ = winrt::Microsoft::UI::Xaml::Controls::TextBlock();

  root_.HorizontalAlignment(
      winrt::Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
  root_.VerticalAlignment(
      winrt::Microsoft::UI::Xaml::VerticalAlignment::Stretch);
  panel_.HorizontalAlignment(
      winrt::Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
  panel_.VerticalAlignment(
      winrt::Microsoft::UI::Xaml::VerticalAlignment::Stretch);

  winrt::Windows::UI::Color slot_bg{255, 28, 42, 58};
  root_.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(slot_bg));

  status_.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
  status_.Margin(winrt::Microsoft::UI::Xaml::Thickness{12, 12, 12, 12});
  status_.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Top);
  status_.IsHitTestVisible(false);
  status_.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(
      winrt::Windows::UI::Colors::White()));

  root_.Children().Append(panel_);
  root_.Children().Append(status_);

  panel_.SizeChanged(
      [this](winrt::Windows::Foundation::IInspectable const&,
             winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&) {
        sync_layout();
      });
  root_.Loaded([this](winrt::Windows::Foundation::IInspectable const&,
                      winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) {
    sync_layout();
  });
  root_.SizeChanged(
      [this](winrt::Windows::Foundation::IInspectable const&,
             winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&) {
        sync_layout();
      });
}

MapHost::~MapHost() {
  stop_present_timer();
  if (session_) {
    session_->SetObserver(nullptr);
  }
  close_all_views();
  destroy_child_hwnd();
}

winrt::Microsoft::UI::Xaml::Controls::Grid MapHost::root_element() const {
  return root_;
}

void MapHost::close_all_views() {
  if (!session_) {
    view_ = nullptr;
    view_id_ = 0;
    for (ViewSlot& slot : slots_) {
      slot = {};
    }
    return;
  }
  for (ViewSlot& slot : slots_) {
    if (slot.view_id != 0) {
      session_->CloseView(slot.view_id);
    }
    slot = {};
  }
  view_ = nullptr;
  view_id_ = 0;
}

void MapHost::attach_session(content::MapContents* session, HWND window_hwnd) {
  if (session_ && session_ != session) {
    session_->SetObserver(nullptr);
    close_all_views();
  }
  session_ = session;
  window_hwnd_ = window_hwnd;
  island_hwnd_ = resolve_island_hwnd();
  if (!session_) {
    status_.Text(L"No MapContents session");
    status_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
    return;
  }
  session_->SetObserver(this);
  show_kind(content::ViewKind::kMapEdit);
}

void MapHost::OnFrameReady(uint32_t view_id, uint32_t generation) {
  if (view_id != view_id_ || !child_hwnd_) {
    return;
  }
  painted_generation_ = generation;
  // Invalidate from the UI thread — this callback runs on the pipe recv
  // thread; posting avoids WinUI / message-pump races that freeze chrome.
  PostMessageW(child_hwnd_, WM_USER + 40, 0, 0);
}

void MapHost::show_kind(content::ViewKind kind) {
  if (!session_) {
    return;
  }
  const int idx = slot_index(kind);
  // Same kind already live: only re-sync HWND to the panel (Views parity —
  // never CloseView/OpenView just because the chrome tab was clicked again).
  if (slots_[idx].view_id != 0 && kind_ == kind && view_ == slots_[idx].view) {
    if (view_) {
      view_->SetVisible(true);
    }
    set_map_surface_visible(true);
    sync_layout();
    update_status_overlay();
    return;
  }

  stop_present_timer();
  if (view_ && view_ != slots_[idx].view) {
    view_->SetVisible(false);
  }

  if (slots_[idx].view_id == 0) {
    slots_[idx].view_id = session_->OpenView(kind);
    // Software DIB: GPU publishes shared pixels; this HWND presents Latest().
    slots_[idx].view =
        session_->AttachSurface(slots_[idx].view_id,
                                content::PresentMode::kSoftwareDib);
  }

  kind_ = kind;
  view_id_ = slots_[idx].view_id;
  view_ = slots_[idx].view;
  painted_generation_ = 0;

  attach_child_hwnd();
  if (view_) {
    view_->SetVisible(true);
  }
  start_present_timer();
  sync_layout();
  update_status_overlay();
}

void MapHost::update_status_overlay() {
  if (!status_) {
    return;
  }
  // Once live pixels fill the slot, hide the diagnostic overlay so it does
  // not look like chrome misalignment over the map surface.
  if (has_live_map_pixels()) {
    status_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
    return;
  }
  status_.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
  wchar_t text[256];
  const wchar_t* kind_name = L"Map Edit";
  if (kind_ == content::ViewKind::kMapData) {
    kind_name = L"Data";
  } else if (kind_ == content::ViewKind::kScene3d) {
    kind_name = L"3D scene";
  }
  swprintf_s(text, L"%s  |  Present: HWND island + DIB  |  %s", kind_name,
             process_path());
  status_.Text(text);
}

HWND MapHost::resolve_island_hwnd() const {
  if (!window_hwnd_) {
    return nullptr;
  }
  EnumIslandCtx ctx;
  EnumChildWindows(window_hwnd_, enum_island_proc,
                   reinterpret_cast<LPARAM>(&ctx));
  return ctx.found ? ctx.found : window_hwnd_;
}

float MapHost::panel_scale() const {
  try {
    if (panel_ && panel_.XamlRoot()) {
      const double scale = panel_.XamlRoot().RasterizationScale();
      if (scale > 0.0) {
        return static_cast<float>(scale);
      }
    }
  } catch (winrt::hresult_error const&) {
  }
  const HWND dpi_hwnd = island_hwnd_ ? island_hwnd_ : window_hwnd_;
  if (dpi_hwnd) {
    const UINT dpi = GetDpiForWindow(dpi_hwnd);
    if (dpi > 0) {
      return static_cast<float>(dpi) / 96.f;
    }
  }
  return 1.f;
}

void MapHost::attach_child_hwnd() {
  if (!window_hwnd_) {
    return;
  }
  if (!island_hwnd_) {
    island_hwnd_ = resolve_island_hwnd();
  }
  HWND parent = island_hwnd_ ? island_hwnd_ : window_hwnd_;
  register_child_class();
  if (child_hwnd_ && GetParent(child_hwnd_) != parent) {
    destroy_child_hwnd();
  }
  if (!child_hwnd_) {
    last_layout_x_ = last_layout_y_ = last_layout_w_ = last_layout_h_ = -1;
    child_hwnd_ = CreateWindowExW(
        0, kChildClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1,
        1, parent, nullptr, GetModuleHandleW(nullptr), this);
  }
  if (view_ && child_hwnd_) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = child_hwnd_;
    view_->Create(params, content::MapWidgetHostView::Preferences());
    view_->SetPresentMode(content::PresentMode::kSoftwareDib);
  }
}

void MapHost::destroy_child_hwnd() {
  stop_present_timer();
  if (child_hwnd_) {
    DestroyWindow(child_hwnd_);
    child_hwnd_ = nullptr;
  }
  last_layout_x_ = last_layout_y_ = last_layout_w_ = last_layout_h_ = -1;
}

void MapHost::start_present_timer() {
  if (child_hwnd_) {
    SetTimer(child_hwnd_, kPresentTimerId, 33, nullptr);
  }
}

void MapHost::stop_present_timer() {
  if (child_hwnd_) {
    KillTimer(child_hwnd_, kPresentTimerId);
  }
}

bool MapHost::has_synced_map_layout() const {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return false;
  }
  RECT rc = {};
  GetClientRect(child_hwnd_, &rc);
  return (rc.right - rc.left) > 8 && (rc.bottom - rc.top) > 8;
}

bool MapHost::has_presented_frame() const {
  if (!view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

bool MapHost::has_live_map_pixels() const {
  if (!has_presented_frame()) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  const SIZE_T bytes = static_cast<SIZE_T>(surface.width_px) *
                       static_cast<SIZE_T>(surface.height_px) * 4u;
  void* bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                             FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                         FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  }
  if (!bits) {
    return false;
  }
  const auto* px = static_cast<const uint8_t*>(bits);
  // GPU map-edit clear is BGRA(0x40,0x80,0xC0,*); placeholder is RGB(28,42,58).
  const bool placeholder = px[0] == 28 && px[1] == 42 && px[2] == 58;
  UnmapViewOfFile(bits);
  return !placeholder;
}

void MapHost::set_map_surface_visible(bool visible) {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return;
  }
  ShowWindow(child_hwnd_, visible ? SW_SHOW : SW_HIDE);
  if (view_) {
    view_->SetVisible(visible);
  }
  if (visible) {
    start_present_timer();
    sync_layout();
  } else {
    stop_present_timer();
  }
}

void MapHost::sync_layout() {
  if (!child_hwnd_ || !panel_) {
    return;
  }
  if (!island_hwnd_) {
    island_hwnd_ = resolve_island_hwnd();
    if (island_hwnd_ && GetParent(child_hwnd_) != island_hwnd_) {
      // Re-parent once the XAML island HWND is known so DIP coords match.
      const HWND old = child_hwnd_;
      child_hwnd_ = nullptr;
      DestroyWindow(old);
      attach_child_hwnd();
      start_present_timer();
      if (!child_hwnd_) {
        return;
      }
    }
  }

  // Force a layout pass so ActualWidth/Height match the chrome grid after
  // AppWindow resize / first Activate (self-test used to see 0x0 briefly).
  try {
    if (root_) {
      root_.UpdateLayout();
    }
    panel_.UpdateLayout();
  } catch (winrt::hresult_error const&) {
  }

  const float w = static_cast<float>(panel_.ActualWidth());
  const float h = static_cast<float>(panel_.ActualHeight());
  if (w < 1.f || h < 1.f) {
    return;
  }

  const float scale = panel_scale();
  // Transform relative to XamlRoot content when available — matches the
  // DesktopChildSiteBridge client origin after re-parenting.
  winrt::Windows::Foundation::Point origin{0.f, 0.f};
  try {
    winrt::Microsoft::UI::Xaml::UIElement relative{nullptr};
    if (panel_.XamlRoot()) {
      relative = panel_.XamlRoot().Content().try_as<
          winrt::Microsoft::UI::Xaml::UIElement>();
    }
    const auto transform =
        relative ? panel_.TransformToVisual(relative)
                 : panel_.TransformToVisual(nullptr);
    origin = transform.TransformPoint({0.f, 0.f});
  } catch (winrt::hresult_error const&) {
    try {
      const auto transform = panel_.TransformToVisual(nullptr);
      origin = transform.TransformPoint({0.f, 0.f});
    } catch (winrt::hresult_error const&) {
      return;
    }
  }

  const int x = static_cast<int>(origin.X * scale + (origin.X >= 0 ? 0.5f : -0.5f));
  const int y = static_cast<int>(origin.Y * scale + (origin.Y >= 0 ? 0.5f : -0.5f));
  const int pw = static_cast<int>(w * scale + 0.5f);
  const int ph = static_cast<int>(h * scale + 0.5f);
  if (pw < 1 || ph < 1) {
    return;
  }

  // Skip no-op SetWindowPos: repeated SWP_SHOWWINDOW thrash causes flicker
  // against XAML chrome (same lesson as ui::views::View::sync_native_bounds).
  RECT wr = {};
  GetWindowRect(child_hwnd_, &wr);
  POINT tl = {wr.left, wr.top};
  if (HWND parent = GetParent(child_hwnd_)) {
    ScreenToClient(parent, &tl);
  }
  const int cur_w = wr.right - wr.left;
  const int cur_h = wr.bottom - wr.top;
  const bool shown = IsWindowVisible(child_hwnd_) != FALSE;
  const bool same = tl.x == x && tl.y == y && cur_w == pw && cur_h == ph && shown;
  const bool size_changed =
      last_layout_w_ != pw || last_layout_h_ != ph || cur_w != pw || cur_h != ph;
  if (!same) {
    SetWindowPos(child_hwnd_, nullptr, x, y, pw, ph,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  last_layout_x_ = x;
  last_layout_y_ = y;
  last_layout_w_ = pw;
  last_layout_h_ = ph;

  if (view_ && (size_changed || !has_presented_frame())) {
    view_->Resize(pw, ph, scale * 96.f);
  }
}

const wchar_t* MapHost::present_path() const {
  return L"HWND island + DIB";
}

const wchar_t* MapHost::process_path() const {
  if (session_ && session_->IsOopRender()) {
    return L"OOP --type=gpu";
  }
  return L"GPU not started";
}

bool MapHost::present_latest_frame(HDC hdc, const RECT& client_rc) const {
  if (!hdc || !view_) {
    return false;
  }
  // Copy Latest under the HostView lock, then map — never hold UI paint
  // across a SetLatest that could replace the handle mid-blit.
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
  return ok != 0 && ok != GDI_ERROR;
}

void MapHost::paint_child() const {
  if (!child_hwnd_) {
    return;
  }
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(child_hwnd_, &ps);
  RECT rc;
  GetClientRect(child_hwnd_, &rc);
  paint_to_dc(hdc, rc);
  EndPaint(child_hwnd_, &ps);
}

void MapHost::paint_to_dc(HDC hdc, const RECT& rc) const {
  if (!hdc) {
    return;
  }
  bool presented = present_latest_frame(hdc, rc);
  if (!presented) {
    const bool scene3d = kind_ == content::ViewKind::kScene3d;
    const HBRUSH brush =
        CreateSolidBrush(scene3d ? RGB(32, 28, 48) : RGB(28, 42, 58));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(230, 236, 242));
    const wchar_t* line1 =
        scene3d ? L"SmartGIS 3D scene (WinUI host)" : L"SmartGIS map (WinUI host)";
    DrawTextW(hdc, line1, -1, const_cast<RECT*>(&rc),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT rc2 = rc;
    rc2.top += 28;
    DrawTextW(hdc, process_path(), -1, &rc2, DT_CENTER | DT_TOP | DT_SINGLELINE);
  }
}

LRESULT CALLBACK MapHost::child_wnd_proc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam) {
  MapHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<MapHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<MapHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (msg == WM_TIMER && wparam == kPresentTimerId && self && self->view_) {
    const content::SharedSurface surface = self->view_->Latest();
    if (surface.generation != 0 &&
        surface.generation != self->painted_generation_) {
      self->painted_generation_ = surface.generation;
      InvalidateRect(hwnd, nullptr, FALSE);
      self->update_status_overlay();
    }
    return 0;
  }

  if (self && self->session_ && self->view_id_ != 0) {
    content::InputEvent ev{};
    ev.x_px = static_cast<int32_t>(GET_X_LPARAM(lparam));
    ev.y_px = static_cast<int32_t>(GET_Y_LPARAM(lparam));
    bool dispatch = false;
    switch (msg) {
      case WM_MOUSEMOVE: {
        // Throttle move IPC — raw move storms blocked Dispatch() on the UI
        // thread when the GPU republished full DIBs (white-screen hang).
        static DWORD last_move_ms = 0;
        const DWORD now = GetTickCount();
        if (now - last_move_ms < 16) {
          break;
        }
        last_move_ms = now;
        ev.kind = content::InputEvent::Kind::kMouseMove;
        dispatch = true;
        break;
      }
      case WM_LBUTTONDOWN:
        ev.kind = content::InputEvent::Kind::kLDown;
        dispatch = true;
        break;
      case WM_LBUTTONUP:
        ev.kind = content::InputEvent::Kind::kLUp;
        dispatch = true;
        break;
      case WM_RBUTTONDOWN:
        ev.kind = content::InputEvent::Kind::kRDown;
        dispatch = true;
        break;
      case WM_RBUTTONUP:
        ev.kind = content::InputEvent::Kind::kRUp;
        dispatch = true;
        break;
      case WM_MOUSEWHEEL: {
        // WM_MOUSEWHEEL lParam is screen coords — convert to client.
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        ev.x_px = pt.x;
        ev.y_px = pt.y;
        ev.kind = content::InputEvent::Kind::kWheel;
        ev.wheel = GET_WHEEL_DELTA_WPARAM(wparam);
        dispatch = true;
        break;
      }
      default:
        break;
    }
    if (dispatch) {
      self->session_->Dispatch(self->view_id_, ev);
    }
  }

  if (msg == WM_USER + 40) {
    InvalidateRect(hwnd, nullptr, FALSE);
    if (self) {
      self->update_status_overlay();
    }
    return 0;
  }

  if (msg == WM_PAINT && self) {
    self->paint_child();
    return 0;
  }
  if (msg == WM_PRINTCLIENT && self) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    self->paint_to_dc(reinterpret_cast<HDC>(wparam), rc);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace winui
}  // namespace app
