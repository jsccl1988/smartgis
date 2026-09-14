// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cs/native/sg_host.h"

#include "content/public/map_contents.h"
#include "content/public/map_contents_observer.h"
#include "content/public/map_widget_host_view.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kChildClass[] = L"SmartGisCsMapHost";
constexpr UINT_PTR kPresentTimerId = 1;

void register_child_class();

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

HWND resolve_island_hwnd(HWND window_hwnd) {
  if (!window_hwnd) {
    return nullptr;
  }
  EnumIslandCtx ctx;
  EnumChildWindows(window_hwnd, enum_island_proc,
                   reinterpret_cast<LPARAM>(&ctx));
  return ctx.found ? ctx.found : window_hwnd;
}

std::wstring dir_of_module(HMODULE mod) {
  wchar_t path[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(mod, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return L".";
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i] = L'\0';
      break;
    }
  }
  return path;
}

std::wstring find_render_exe(const std::wstring& dir) {
  const wchar_t* names[] = {L"SmartGisRender.exe", L"SmartGisRenderD.exe"};
  for (const wchar_t* name : names) {
    const std::wstring cand = dir + L"\\" + name;
    const DWORD attrs = GetFileAttributesW(cand.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES &&
        (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return std::wstring();
}

void apply_gpu_exe_override() {
  HMODULE self = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&sg_host_create), &self);
  std::wstring found = find_render_exe(dir_of_module(self));
  if (found.empty()) {
    found = find_render_exe(dir_of_module(nullptr));
  }
  if (!found.empty()) {
    content::MapContents::SetGpuExeOverride(found.c_str());
  }
}

content::ViewKind kind_from_int(int kind) {
  if (kind == 1) {
    return content::ViewKind::kMapData;
  }
  if (kind == 2) {
    return content::ViewKind::kScene3d;
  }
  return content::ViewKind::kMapEdit;
}

int int_from_kind(content::ViewKind kind) {
  if (kind == content::ViewKind::kMapData) {
    return 1;
  }
  if (kind == content::ViewKind::kScene3d) {
    return 2;
  }
  return 0;
}

}  // namespace

struct SgHost : public content::MapContentsObserver {
  struct ViewSlot {
    uint32_t view_id = 0;
    content::MapWidgetHostView* view = nullptr;
  };

  SgHost() {
    apply_gpu_exe_override();
    session = content::MapContents::Create();
    if (session) {
      session->SetObserver(this);
    }
  }

  ~SgHost() override {
    destroy_child_hwnd();
    if (session) {
      session->SetObserver(nullptr);
      session->Shutdown();
      delete session;
      session = nullptr;
    }
  }

  void OnFrameReady(uint32_t view_id, uint32_t generation) override {
    if (view_id != view_id_ || !child_hwnd_) {
      return;
    }
    painted_generation_ = generation;
    PostMessageW(child_hwnd_, WM_USER + 40, 0, 0);
  }

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam);

  void attach_child_hwnd();
  void destroy_child_hwnd();
  void start_present_timer();
  void stop_present_timer();
  void paint_child() const;
  void paint_to_dc(HDC hdc, const RECT& rc) const;
  bool present_latest_frame(HDC hdc, const RECT& client_rc) const;
  void show_kind(content::ViewKind kind);
  static int slot_index(content::ViewKind kind);

  content::MapContents* session = nullptr;
  ViewSlot slots_[3] = {};
  HWND window_hwnd_ = nullptr;
  HWND island_hwnd_ = nullptr;
  HWND child_hwnd_ = nullptr;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  int last_x_ = -1;
  int last_y_ = -1;
  int last_w_ = -1;
  int last_h_ = -1;
  content::ViewKind kind_ = content::ViewKind::kMapEdit;
  content::MapWidgetHostView* view_ = nullptr;
};

int SgHost::slot_index(content::ViewKind kind) {
  return int_from_kind(kind);
}

void SgHost::attach_child_hwnd() {
  if (!window_hwnd_) {
    return;
  }
  if (!island_hwnd_) {
    island_hwnd_ = resolve_island_hwnd(window_hwnd_);
  }
  HWND parent = island_hwnd_ ? island_hwnd_ : window_hwnd_;
  register_child_class();
  if (child_hwnd_ && GetParent(child_hwnd_) != parent) {
    destroy_child_hwnd();
  }
  if (!child_hwnd_) {
    last_x_ = last_y_ = last_w_ = last_h_ = -1;
    child_hwnd_ = CreateWindowExW(0, kChildClass, L"",
                                  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0,
                                  1, 1, parent, nullptr,
                                  GetModuleHandleW(nullptr), this);
  }
  if (view_ && child_hwnd_) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = child_hwnd_;
    view_->Create(params, content::MapWidgetHostView::Preferences());
    view_->SetPresentMode(content::PresentMode::kSoftwareDib);
  }
}

void SgHost::destroy_child_hwnd() {
  stop_present_timer();
  if (child_hwnd_) {
    DestroyWindow(child_hwnd_);
    child_hwnd_ = nullptr;
  }
  last_x_ = last_y_ = last_w_ = last_h_ = -1;
}

void SgHost::start_present_timer() {
  if (child_hwnd_) {
    SetTimer(child_hwnd_, kPresentTimerId, 33, nullptr);
  }
}

void SgHost::stop_present_timer() {
  if (child_hwnd_) {
    KillTimer(child_hwnd_, kPresentTimerId);
  }
}

void SgHost::show_kind(content::ViewKind kind) {
  if (!session) {
    return;
  }
  const int idx = slot_index(kind);
  if (view_ && view_ != slots_[idx].view) {
    view_->SetVisible(false);
  }
  if (slots_[idx].view_id == 0) {
    slots_[idx].view_id = session->OpenView(kind);
    slots_[idx].view = session->AttachSurface(
        slots_[idx].view_id, content::PresentMode::kSoftwareDib);
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
}

bool SgHost::present_latest_frame(HDC hdc, const RECT& client_rc) const {
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
  void* bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                             FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                         FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  }
  if (!bits) {
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

void SgHost::paint_to_dc(HDC hdc, const RECT& rc) const {
  if (!hdc) {
    return;
  }
  if (present_latest_frame(hdc, rc)) {
    return;
  }
  const bool scene3d = kind_ == content::ViewKind::kScene3d;
  const HBRUSH brush =
      CreateSolidBrush(scene3d ? RGB(32, 28, 48) : RGB(28, 42, 58));
  FillRect(hdc, &rc, brush);
  DeleteObject(brush);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(230, 236, 242));
  const wchar_t* line1 =
      scene3d ? L"SmartGIS 3D scene (C# host)" : L"SmartGIS map (C# host)";
  DrawTextW(hdc, line1, -1, const_cast<RECT*>(&rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void SgHost::paint_child() const {
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

LRESULT CALLBACK SgHost::child_wnd_proc(HWND hwnd,
                                        UINT msg,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  SgHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<SgHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<SgHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (msg == WM_TIMER && wparam == kPresentTimerId && self && self->view_) {
    const content::SharedSurface surface = self->view_->Latest();
    if (surface.generation != 0 &&
        surface.generation != self->painted_generation_) {
      self->painted_generation_ = surface.generation;
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  }

  if (self && self->session && self->view_id_ != 0) {
    content::InputEvent ev{};
    ev.x_px = static_cast<int32_t>(GET_X_LPARAM(lparam));
    ev.y_px = static_cast<int32_t>(GET_Y_LPARAM(lparam));
    bool dispatch = false;
    switch (msg) {
      case WM_MOUSEMOVE: {
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
      self->session->Dispatch(self->view_id_, ev);
    }
  }

  if (msg == WM_USER + 40) {
    InvalidateRect(hwnd, nullptr, FALSE);
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

namespace {

void register_child_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = SgHost::child_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kChildClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

extern "C" {

SgHost* sg_host_create(void) {
  return new SgHost();
}

void sg_host_destroy(SgHost* host) {
  delete host;
}

int sg_host_start_render(SgHost* host) {
  if (!host || !host->session) {
    return 0;
  }
  return host->session->StartRenderProcess() ? 1 : 0;
}

int sg_host_is_oop(const SgHost* host) {
  return host && host->session && host->session->IsOopRender() ? 1 : 0;
}

const wchar_t* sg_host_present_status(const SgHost* host) {
  if (!host || !host->session) {
    return L"down";
  }
  return host->session->PresentStatus();
}

uint32_t sg_host_open_view(SgHost* host, int kind) {
  if (!host) {
    return 0;
  }
  host->show_kind(kind_from_int(kind));
  return host->view_id_;
}

void sg_host_show_kind(SgHost* host, int kind) {
  if (!host) {
    return;
  }
  host->show_kind(kind_from_int(kind));
}

uint32_t sg_host_view_id(const SgHost* host) {
  return host ? host->view_id_ : 0;
}

int sg_host_view_kind(const SgHost* host) {
  return host ? int_from_kind(host->kind_) : 0;
}

void sg_host_attach_parent(SgHost* host, void* hwnd) {
  if (!host) {
    return;
  }
  host->window_hwnd_ = static_cast<HWND>(hwnd);
  host->island_hwnd_ = nullptr;
  host->attach_child_hwnd();
}

void sg_host_sync_layout(SgHost* host, int x, int y, int w, int h, float dpi) {
  if (!host || !host->child_hwnd_ || w < 1 || h < 1) {
    return;
  }
  if (!host->island_hwnd_ && host->window_hwnd_) {
    host->island_hwnd_ = resolve_island_hwnd(host->window_hwnd_);
    if (host->island_hwnd_ &&
        GetParent(host->child_hwnd_) != host->island_hwnd_) {
      const HWND old = host->child_hwnd_;
      host->child_hwnd_ = nullptr;
      DestroyWindow(old);
      host->attach_child_hwnd();
      host->start_present_timer();
      if (!host->child_hwnd_) {
        return;
      }
    }
  }
  const bool same = host->last_x_ == x && host->last_y_ == y &&
                    host->last_w_ == w && host->last_h_ == h &&
                    IsWindowVisible(host->child_hwnd_);
  if (!same) {
    SetWindowPos(host->child_hwnd_, nullptr, x, y, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(host->child_hwnd_, nullptr, FALSE);
  }
  host->last_x_ = x;
  host->last_y_ = y;
  host->last_w_ = w;
  host->last_h_ = h;
  if (host->view_) {
    const float scale = dpi > 1.f ? dpi : 96.f;
    host->view_->Resize(w, h, scale);
  }
}

void sg_host_set_visible(SgHost* host, int visible) {
  if (!host || !host->child_hwnd_ || !IsWindow(host->child_hwnd_)) {
    return;
  }
  ShowWindow(host->child_hwnd_, visible ? SW_SHOW : SW_HIDE);
  if (host->view_) {
    host->view_->SetVisible(visible != 0);
  }
  if (visible) {
    host->start_present_timer();
  } else {
    host->stop_present_timer();
  }
}

void* sg_host_map_child_hwnd(const SgHost* host) {
  return host ? host->child_hwnd_ : nullptr;
}

int sg_host_has_synced_layout(const SgHost* host) {
  if (!host || !host->child_hwnd_ || !IsWindow(host->child_hwnd_)) {
    return 0;
  }
  RECT rc = {};
  GetClientRect(host->child_hwnd_, &rc);
  return (rc.right - rc.left) > 8 && (rc.bottom - rc.top) > 8 ? 1 : 0;
}

int sg_host_has_presented_frame(const SgHost* host) {
  if (!host || !host->view_) {
    return 0;
  }
  const content::SharedSurface surface = host->view_->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
                 surface.width_px >= 8 && surface.height_px >= 8
             ? 1
             : 0;
}

int sg_host_has_live_pixels(const SgHost* host) {
  if (!sg_host_has_presented_frame(host)) {
    return 0;
  }
  const content::SharedSurface surface = host->view_->Latest();
  const SIZE_T bytes = static_cast<SIZE_T>(surface.width_px) *
                       static_cast<SIZE_T>(surface.height_px) * 4u;
  void* bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                             FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    bits = MapViewOfFile(static_cast<HANDLE>(surface.nt_handle),
                         FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  }
  if (!bits) {
    return 0;
  }
  const auto* px = static_cast<const uint8_t*>(bits);
  const bool placeholder = px[0] == 28 && px[1] == 42 && px[2] == 58;
  UnmapViewOfFile(bits);
  return placeholder ? 0 : 1;
}

void sg_host_catalog_call(SgHost* host, const char* json) {
  if (!host || !host->session || !json || !json[0]) {
    return;
  }
  host->session->CatalogCall(json);
}

void sg_host_activate_tool(SgHost* host, const char* tool_id) {
  if (!host || !host->session || host->view_id_ == 0 || !tool_id || !tool_id[0]) {
    return;
  }
  host->session->ActivateTool(host->view_id_, tool_id);
}

int sg_host_wait_frame(SgHost* host, uint32_t timeout_ms) {
  if (!host || !host->session || host->view_id_ == 0) {
    return 0;
  }
  if (!host->session->WaitFrameReady(host->view_id_, timeout_ms)) {
    return 0;
  }
  return sg_host_has_presented_frame(host) && sg_host_has_live_pixels(host);
}

}  // extern "C"
