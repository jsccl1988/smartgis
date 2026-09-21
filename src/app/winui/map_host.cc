// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/map_host.h"

#include "app/views/map_host_extent.h"
#include "app/views/scene3d_rhi_session.h"
#include "tool/camera_nav.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include <cstring>
#include <string>
#include <vector>

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
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
  // Do not seed China OGR packs here. OnLaunched constructs MapHost on the
  // Xaml UI stack; ingest_ogr_path of china_city has been observed to free an
  // invalid heap block (RtlValidateHeap / STATUS_HEAP_CORRUPTION) before the
  // window is fully up. attach_session seeds when feature_count() == 0.
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
  // Layout-only slot: no DXGI swap chain is attached. Keep the panel
  // transparent so a late HWND does not flash an empty white surface.
  panel_.Opacity(0);

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
        if (!shutting_down_) {
          sync_layout();
        }
      });
  root_.Loaded([this](winrt::Windows::Foundation::IInspectable const&,
                      winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) {
    if (!shutting_down_) {
      sync_layout();
    }
  });
  root_.SizeChanged(
      [this](winrt::Windows::Foundation::IInspectable const&,
             winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&) {
        if (!shutting_down_) {
          sync_layout();
        }
      });
  wire_panel_pointers();
}

MapHost::~MapHost() {
  begin_shutdown();
}

void MapHost::begin_shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;
  stop_present_timer();
  if (child_hwnd_ && IsWindow(child_hwnd_)) {
    KillTimer(child_hwnd_, kBlitTimerId);
  }
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
  if (shutting_down_) {
    return;
  }
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
    // Still create the overlay so seed MapScene can paint without GPU.
    attach_child_hwnd();
    start_present_timer();
    sync_layout();
    return;
  }
  // Views parity: seed China PLP (polygon/line/point + labels) in-process.
  if (map_scene_.feature_count() == 0) {
    map_scene_.seed_default();
  }
  session_->SetObserver(this);
  scene3d_.bind_map(&map_scene_);
  show_kind(content::ViewKind::kMapEdit);
}

void MapHost::set_active_tool(std::string_view tool_id) {
  if (tool_id.empty()) {
    return;
  }
  active_tool_.assign(tool_id.begin(), tool_id.end());
  if (active_tool_ == "selection.clear") {
    map_scene_.clear_selection();
    if (child_hwnd_) {
      InvalidateRect(child_hwnd_, nullptr, FALSE);
    }
  }
}

void MapHost::apply_pointer(const content::InputEvent& ev) {
  RECT rc = {};
  int w = last_layout_w_;
  int h = last_layout_h_;
  if (child_hwnd_ && IsWindow(child_hwnd_)) {
    GetClientRect(child_hwnd_, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
  }
  if (w < 8 || h < 8) {
    return;
  }

  if (ev.kind == content::InputEvent::Kind::kWheel) {
    const double factor = ev.wheel > 0 ? 1.15 : (1.0 / 1.15);
    blit_.begin_zoom(w, h, ev.x_px, ev.y_px, factor);
    map_scene_.apply_zoom_at(ev.x_px, ev.y_px, factor);
    if (child_hwnd_) {
      InvalidateRect(child_hwnd_, nullptr, FALSE);
      schedule_full_redraw();
    }
    return;
  }

  const bool is_pan = active_tool_.empty() || active_tool_ == "view.pan" ||
                      active_tool_ == "view3d.trackball";
  const bool is_select =
      active_tool_ == "selection.point" || active_tool_ == "select" ||
      active_tool_ == "identify";
  const bool is_zoom_in = active_tool_ == "view.zoom_in";
  const bool is_zoom_out = active_tool_ == "view.zoom_out";

  if (ev.kind == content::InputEvent::Kind::kLDown) {
    dragging_ = true;
    last_pointer_x_ = ev.x_px;
    last_pointer_y_ = ev.y_px;
    if (child_hwnd_) {
      SetCapture(child_hwnd_);
    }
    // Pan press must not Invalidate — a full MapScene rebuild flashes white
    // and races the GPU DIB present path. Only tools that change visuals repaint.
    if (is_select) {
      map_scene_.hit_test(ev.x_px, ev.y_px, w, h);
      if (child_hwnd_) {
        InvalidateRect(child_hwnd_, nullptr, FALSE);
      }
    } else if (is_zoom_in) {
      blit_.begin_zoom(w, h, ev.x_px, ev.y_px, 1.15);
      map_scene_.apply_zoom_at(ev.x_px, ev.y_px, 1.15);
      schedule_full_redraw();
      if (child_hwnd_) {
        InvalidateRect(child_hwnd_, nullptr, FALSE);
      }
    } else if (is_zoom_out) {
      blit_.begin_zoom(w, h, ev.x_px, ev.y_px, 1.0 / 1.15);
      map_scene_.apply_zoom_at(ev.x_px, ev.y_px, 1.0 / 1.15);
      schedule_full_redraw();
      if (child_hwnd_) {
        InvalidateRect(child_hwnd_, nullptr, FALSE);
      }
    }
    return;
  }
  if (ev.kind == content::InputEvent::Kind::kLUp ||
      ev.kind == content::InputEvent::Kind::kRUp) {
    dragging_ = false;
    ReleaseCapture();
    return;
  }
  if (ev.kind == content::InputEvent::Kind::kMouseMove && dragging_ && is_pan) {
    const int dx = ev.x_px - last_pointer_x_;
    const int dy = ev.y_px - last_pointer_y_;
    blit_.begin_pan(w, h, dx, dy);
    map_scene_.apply_pan(dx, dy);
    last_pointer_x_ = ev.x_px;
    last_pointer_y_ = ev.y_px;
    if (child_hwnd_) {
      InvalidateRect(child_hwnd_, nullptr, FALSE);
      schedule_full_redraw();
    }
  }
}

void MapHost::schedule_full_redraw() {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return;
  }
  KillTimer(child_hwnd_, kBlitTimerId);
  SetTimer(child_hwnd_, kBlitTimerId,
           static_cast<UINT>(tool::kBlitDebounceMs), nullptr);
}

void MapHost::commit_blit_preview() {
  blit_.end_preview();
  if (child_hwnd_) {
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
}

void MapHost::wire_panel_pointers() {
  using winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs;
  panel_.PointerPressed(
      [this](winrt::Windows::Foundation::IInspectable const&,
             PointerRoutedEventArgs const& e) {
        if (shutting_down_) {
          return;
        }
        const auto pt = e.GetCurrentPoint(panel_);
        const float scale = panel_scale();
        content::InputEvent ev{};
        ev.kind = pt.Properties().IsRightButtonPressed()
                      ? content::InputEvent::Kind::kRDown
                      : content::InputEvent::Kind::kLDown;
        ev.x_px = static_cast<int32_t>(pt.Position().X * scale);
        ev.y_px = static_cast<int32_t>(pt.Position().Y * scale);
        apply_pointer(ev);
        if (session_ && view_id_ != 0) {
          session_->Dispatch(view_id_, ev);
        }
      });
  panel_.PointerMoved(
      [this](winrt::Windows::Foundation::IInspectable const&,
             PointerRoutedEventArgs const& e) {
        if (shutting_down_) {
          return;
        }
        const auto pt = e.GetCurrentPoint(panel_);
        const float scale = panel_scale();
        content::InputEvent ev{};
        ev.kind = content::InputEvent::Kind::kMouseMove;
        ev.x_px = static_cast<int32_t>(pt.Position().X * scale);
        ev.y_px = static_cast<int32_t>(pt.Position().Y * scale);
        apply_pointer(ev);
        if (session_ && view_id_ != 0) {
          session_->Dispatch(view_id_, ev);
        }
      });
  panel_.PointerReleased(
      [this](winrt::Windows::Foundation::IInspectable const&,
             PointerRoutedEventArgs const& e) {
        if (shutting_down_) {
          return;
        }
        const auto pt = e.GetCurrentPoint(panel_);
        const float scale = panel_scale();
        content::InputEvent ev{};
        ev.kind = pt.Properties().IsRightButtonPressed()
                      ? content::InputEvent::Kind::kRUp
                      : content::InputEvent::Kind::kLUp;
        ev.x_px = static_cast<int32_t>(pt.Position().X * scale);
        ev.y_px = static_cast<int32_t>(pt.Position().Y * scale);
        apply_pointer(ev);
        if (session_ && view_id_ != 0) {
          session_->Dispatch(view_id_, ev);
        }
      });
  panel_.PointerWheelChanged(
      [this](winrt::Windows::Foundation::IInspectable const&,
             PointerRoutedEventArgs const& e) {
        if (shutting_down_) {
          return;
        }
        const auto pt = e.GetCurrentPoint(panel_);
        const float scale = panel_scale();
        content::InputEvent ev{};
        ev.kind = content::InputEvent::Kind::kWheel;
        ev.x_px = static_cast<int32_t>(pt.Position().X * scale);
        ev.y_px = static_cast<int32_t>(pt.Position().Y * scale);
        ev.wheel = pt.Properties().MouseWheelDelta();
        apply_pointer(ev);
        if (session_ && view_id_ != 0) {
          session_->Dispatch(view_id_, ev);
        }
      });
}

bool MapHost::open_map_path(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  const bool ok = map_scene_.open_path(path);
  if (child_hwnd_) {
    RECT rc = {};
    GetClientRect(child_hwnd_, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w > 8 && h > 8) {
      map_scene_.fit_extent(w, h);
    }
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  update_status_overlay();
  return ok;
}

void MapHost::OnFrameReady(uint32_t view_id, uint32_t generation) {
  if (view_id != view_id_ || !child_hwnd_) {
    return;
  }
  painted_generation_ = generation;
  // Invalidate from the UI thread -- this callback runs on the pipe recv
  // thread; posting avoids WinUI / message-pump races that freeze chrome.
  PostMessageW(child_hwnd_, WM_USER + 40, 0, 0);
}

void MapHost::show_kind(content::ViewKind kind) {
  if (shutting_down_ || !session_) {
    return;
  }
  const int idx = slot_index(kind);
  // Same kind already live: only re-sync HWND to the panel (Views parity --
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

  kind_ = kind;
  bool flycube_live = false;
  if (kind == content::ViewKind::kScene3d &&
      app::prefer_scene3d_flycube()) {
    attach_child_hwnd();
    if (scene3d_rhi_.try_attach(child_hwnd_)) {
      flycube_live = true;
    } else {
      scene3d_rhi_.release();
    }
  } else {
    scene3d_rhi_.release();
  }

  // Always OpenView for Scene3d, including FlyCube present_gpu. Self-test
  // wait_frame_ok requires view_id != 0 and WaitFrameReady; early-return
  // without OpenView left view_id_=0 (wait-precheck-fail / exit 10).
  if (slots_[idx].view_id == 0) {
    slots_[idx].view_id = session_->OpenView(kind);
    // Software DIB: GPU publishes shared pixels; this HWND presents Latest().
    slots_[idx].view =
        session_->AttachSurface(slots_[idx].view_id,
                                content::PresentMode::kSoftwareDib);
  }

  view_id_ = slots_[idx].view_id;
  view_ = slots_[idx].view;
  painted_generation_ = 0;

  // Views parity: frame China (or MapScene world) so GPU / 3D DEM match
  // SmartGis.exe leftover extents instead of an empty default.
  {
    content::Extent2 e = map_scene_.world_extent();
    if (!app::extent_looks_like_china(e)) {
      e = app::kChinaLonLatExtent;
    }
    if (view_id_ != 0) {
      session_->SetExtent(view_id_, e);
    }
    scene3d_.bind_contents(session_, view_id_);
    scene3d_.apply_world_extent(e);
  }

  if (!flycube_live) {
    attach_child_hwnd();
    if (kind == content::ViewKind::kScene3d && child_hwnd_) {
      (void)scene3d_stereo_.try_attach(child_hwnd_);
    }
  } else {
    scene3d_stereo_.release();
  }
  if (kind != content::ViewKind::kScene3d) {
    scene3d_stereo_.release();
  }
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
  // Do not fall back to the top-level HWND: DIP?px coords are island-client
  // space and must be MapWindowPoints'd onto the top-level client.
  return ctx.found;
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
  island_hwnd_ = resolve_island_hwnd();
  register_child_class();
  // Owned WS_POPUP (not WS_CHILD): screenshot proof that a top-level WS_CHILD
  // sibling is fully covered by DesktopChildSiteBridge DirectComposition, so
  // the user sees only the XAML slot background while PrintWindow(child) still
  // shows the GPU DIB + MapScene.
  if (child_hwnd_) {
    const LONG_PTR style = GetWindowLongPtrW(child_hwnd_, GWL_STYLE);
    const HWND owner = GetWindow(child_hwnd_, GW_OWNER);
    if ((style & WS_POPUP) == 0 || owner != window_hwnd_) {
      destroy_child_hwnd();
    }
  }
  if (!child_hwnd_) {
    last_layout_x_ = last_layout_y_ = last_layout_w_ = last_layout_h_ = -1;
    child_hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kChildClass, L"",
        WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1, window_hwnd_,
        nullptr, GetModuleHandleW(nullptr), this);
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
  scene3d_stereo_.release();
  scene3d_rhi_.release();
  if (child_hwnd_) {
    // Clear userdata before DestroyWindow so nested WM_TIMER / WM_PAINT
    // during teardown cannot touch a half-destroyed MapHost.
    SetWindowLongPtrW(child_hwnd_, GWLP_USERDATA, 0);
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
  // FlyCube Scene3d paints via present_gpu; DIB generation may stay 0 while
  // the HWND swapchain has already presented a live frame.
  if (kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live() &&
      scene3d_rhi_.last_present_ok()) {
    return true;
  }
  if (!view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

bool MapHost::has_live_map_pixels() const {
  // Product map content is MapScene vectors (China PLP). GPU DIB alone is only
  // a clear/demo base - treat loaded features as live for chrome/self-test.
  if (map_scene_.feature_count() >= 3) {
    return true;
  }
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
  if (shutting_down_ || !child_hwnd_ || !panel_ || !window_hwnd_) {
    return;
  }
  // Heal tiny unpackaged frames (DPI 240 often boots at ~512x320 and collapses
  // the map row to ~25px). Owned popup cannot show a usable map until the
  // owner is large enough for Catalog+Ambox+map.
  {
    HWND root = GetWindow(child_hwnd_, GW_OWNER);
    if (!root) {
      root = window_hwnd_;
    }
    RECT fr = {};
    if (root && GetWindowRect(root, &fr)) {
      const int fw = fr.right - fr.left;
      const int fh = fr.bottom - fr.top;
      if (fw < 1024 || fh < 700) {
        SetWindowPos(root, nullptr, 0, 0, 1280, 800,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
      }
    }
  }
  if (!island_hwnd_) {
    island_hwnd_ = resolve_island_hwnd();
  }
  if (!island_hwnd_) {
    // XAML island HWND not ready yet -- SizeChanged / Loaded will retry.
    return;
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
  // DIP origin relative to XamlRoot content (= island client origin after
  // scale). Then MapWindowPoints onto the top-level client (child parent).
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

  const int x_island =
      static_cast<int>(origin.X * scale + (origin.X >= 0 ? 0.5f : -0.5f));
  const int y_island =
      static_cast<int>(origin.Y * scale + (origin.Y >= 0 ? 0.5f : -0.5f));
  const int pw = static_cast<int>(w * scale + 0.5f);
  const int ph = static_cast<int>(h * scale + 0.5f);
  if (pw < 1 || ph < 1) {
    return;
  }

  POINT pts[2] = {{x_island, y_island}, {x_island + pw, y_island + ph}};
  // Screen coords for owned WS_POPUP (island client ? desktop).
  MapWindowPoints(island_hwnd_, nullptr, pts, 2);
  const int x = pts[0].x;
  const int y = pts[0].y;
  const int mapped_w = pts[1].x - pts[0].x;
  const int mapped_h = pts[1].y - pts[0].y;
  const int use_w = mapped_w > 0 ? mapped_w : pw;
  const int use_h = mapped_h > 0 ? mapped_h : ph;

  // Skip no-op SetWindowPos: repeated SWP_SHOWWINDOW thrash causes flicker.
  RECT wr = {};
  GetWindowRect(child_hwnd_, &wr);
  const int cur_w = wr.right - wr.left;
  const int cur_h = wr.bottom - wr.top;
  const bool shown = IsWindowVisible(child_hwnd_) != FALSE;
  const bool same =
      wr.left == x && wr.top == y && cur_w == use_w && cur_h == use_h && shown;
  const bool size_changed = last_layout_w_ != use_w || last_layout_h_ != use_h ||
                            cur_w != use_w || cur_h != use_h;
  if (!same) {
    SetWindowPos(child_hwnd_, HWND_TOP, x, y, use_w, use_h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  last_layout_x_ = x;
  last_layout_y_ = y;
  last_layout_w_ = use_w;
  last_layout_h_ = use_h;

  if (view_ && (size_changed || !has_presented_frame())) {
    view_->Resize(use_w, use_h, scale * 96.f);
  }
  if (size_changed) {
    if (map_scene_.feature_count() > 0) {
      map_scene_.fit_extent(use_w, use_h);
      scene3d_.apply_world_extent(map_scene_.world_extent());
    }
    if (scene3d_rhi_.is_live() && child_hwnd_ && use_w > 0 && use_h > 0) {
      scene3d_rhi_.resize(child_hwnd_, static_cast<uint32_t>(use_w),
                          static_cast<uint32_t>(use_h));
    }
    // GL viewport is whatever size the child had at attach (often 1x1).
    // Grow it with the popup; present also resizes, but layout is the first
    // moment the client is real.
    if (kind_ == content::ViewKind::kScene3d && child_hwnd_ && use_w > 8 &&
        use_h > 8 && !app::prefer_scene3d_flycube()) {
      if (!scene3d_stereo_.is_live()) {
        (void)scene3d_stereo_.try_attach(child_hwnd_);
      }
      scene3d_stereo_.resize(use_w, use_h);
    }
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  // Kick one FlyCube present during layout only when FlyCube is preferred.
  if (kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live() &&
      app::prefer_scene3d_flycube() && child_hwnd_ && use_w > 8 && use_h > 8) {
    (void)scene3d_rhi_.present(&scene3d_, static_cast<uint32_t>(use_w),
                               static_cast<uint32_t>(use_h));
    UpdateWindow(child_hwnd_);
  }
}

const wchar_t* MapHost::present_path() const {
  if (kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live()) {
    return L"HWND island + FlyCube present_gpu";
  }
  return L"HWND island + DIB + MapScene";
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
  // Copy Latest under the HostView lock, then map -- never hold UI paint
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
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  const bool flycube_scene =
      kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live() &&
      app::prefer_scene3d_flycube();
  // Leftover GL presents to this HWND. Do not cover SwapBuffers with a DIB.
  if (kind_ == content::ViewKind::kScene3d && !flycube_scene && w > 0 &&
      h > 0) {
    if (scene3d_stereo_.try_present_sot(child_hwnd_, hdc, w, h, scene3d_.yaw(),
                                        scene3d_.pitch(),
                                        scene3d_.distance())) {
      EndPaint(child_hwnd_, &ps);
      return;
    }
  }
  if (!flycube_scene && w > 0 && h > 0) {
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
        mem ? CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0)
            : nullptr;
    if (mem && bmp) {
      HGDIOBJ old = SelectObject(mem, bmp);
      paint_to_dc(mem, rc);
      BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
      SelectObject(mem, old);
      DeleteObject(bmp);
      DeleteDC(mem);
    } else {
      if (mem) {
        DeleteDC(mem);
      }
      paint_to_dc(hdc, rc);
    }
  } else {
    paint_to_dc(hdc, rc);
  }
  EndPaint(child_hwnd_, &ps);
}

void MapHost::paint_to_dc(HDC hdc, const RECT& rc) const {
  if (!hdc) {
    return;
  }
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (kind_ == content::ViewKind::kScene3d && scene3d_rhi_.is_live() &&
      app::prefer_scene3d_flycube() && w > 0 && h > 0) {
    const bool ok = scene3d_rhi_.present(
        const_cast<::app::Scene3dController*>(&scene3d_),
        static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    if (ok) {
      scene3d_.paint_hud(hdc, w, h);
      return;
    }
  }
  if (kind_ != content::ViewKind::kScene3d && blit_.in_preview() &&
      blit_.present(hdc, w, h)) {
    return;
  }
  // Product 2D content is MapScene (China PLP). Do not present a GPU DIB and
  // then wipe it with a white overlay — that was the click-refresh flicker.
  const bool map_owns_frame = kind_ != content::ViewKind::kScene3d &&
                              map_scene_.feature_count() > 0 && w > 0 && h > 0;
  if (kind_ == content::ViewKind::kScene3d) {
    // Leftover GL stereo (SoT) on the child HWND. Memory DCs are BitBlt back
    // over SwapBuffers, so they get the GDI DEM only.
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
  bool presented = false;
  if (!map_owns_frame) {
    presented = present_latest_frame(hdc, rc);
  }
  if (!presented && !map_owns_frame) {
    const HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
  }
  // 2D panes: MapScene paints ocean + vectors.
  if (map_owns_frame) {
    map_scene_.paint(hdc, w, h, /*fill_background=*/true);
  } else if (!presented) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(60, 70, 80));
    const wchar_t* line1 = L"SmartGIS map (WinUI host)";
    DrawTextW(hdc, line1, -1, const_cast<RECT*>(&rc),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT rc2 = rc;
    rc2.top += 28;
    DrawTextW(hdc, process_path(), -1, &rc2, DT_CENTER | DT_TOP | DT_SINGLELINE);
  }
  if (w > 0 && h > 0) {
    blit_.capture(hdc, w, h);
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

  if (msg == WM_TIMER && wparam == kBlitTimerId && self) {
    KillTimer(hwnd, kBlitTimerId);
    self->commit_blit_preview();
    return 0;
  }

  if (msg == WM_TIMER && wparam == kPresentTimerId && self) {
    if (self->shutting_down_) {
      return 0;
    }
    // Re-sync screen position when the owner window is dragged / DPI changes.
    self->sync_layout();
    if (self->kind_ == content::ViewKind::kScene3d && IsWindowVisible(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    if (self->view_) {
      const content::SharedSurface surface = self->view_->Latest();
      if (surface.generation != 0 &&
          surface.generation != self->painted_generation_) {
        self->painted_generation_ = surface.generation;
        // MapScene owns the 2D frame — GPU DIB generation churn must not
        // rebuild ~2k vectors at ~30 Hz. Scene3d SoT DIB must still refresh.
        if (self->kind_ == content::ViewKind::kScene3d ||
            self->map_scene_.feature_count() == 0) {
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        self->update_status_overlay();
      }
    }
    return 0;
  }

  if (self && !self->shutting_down_ && self->session_ && self->view_id_ != 0) {
    content::InputEvent ev{};
    ev.x_px = static_cast<int32_t>(GET_X_LPARAM(lparam));
    ev.y_px = static_cast<int32_t>(GET_Y_LPARAM(lparam));
    bool dispatch = false;
    switch (msg) {
      case WM_MOUSEMOVE: {
        // Throttle move IPC -- raw move storms blocked Dispatch() on the UI
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
        // WM_MOUSEWHEEL lParam is screen coords -- convert to client.
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        ev.x_px = pt.x;
        ev.y_px = pt.y;
        ev.kind = content::InputEvent::Kind::kWheel;
        ev.wheel = GET_WHEEL_DELTA_WPARAM(wparam);
        dispatch = true;
        break;
      }
      case WM_MOUSEHWHEEL: {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        ev.x_px = pt.x;
        ev.y_px = pt.y;
        ev.kind = content::InputEvent::Kind::kWheel;
        ev.wheel = GET_WHEEL_DELTA_WPARAM(wparam);
        ev.flags = content::input_flags::kHorizontalWheel;
        dispatch = true;
        break;
      }
      default:
        break;
    }
    if (dispatch) {
      self->apply_pointer(ev);
      self->session_->Dispatch(self->view_id_, ev);
    }
  }

  if (msg == WM_USER + 40) {
    // OnFrameReady from the GPU pipe: only repaint when MapScene is empty.
    if (self && self->map_scene_.feature_count() == 0) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    if (self) {
      self->update_status_overlay();
    }
    return 0;
  }

  if (msg == WM_PAINT && self) {
    if (!self->shutting_down_) {
      self->paint_child();
    } else {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
    }
    return 0;
  }
  if (msg == WM_PRINTCLIENT && self && !self->shutting_down_) {
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
