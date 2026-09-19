// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cs/native/sg_host.h"

#include "app/views/blit_frame_cache.h"
#include "app/views/map_host_extent.h"
#include "app/views/map_scene.h"
#include "app/views/scene3d_controller.h"
#include "content/public/map_contents.h"
#include "tool/camera_nav.h"
#include "content/public/map_contents_observer.h"
#include "content/public/map_widget_host_view.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kChildClass[] = L"SmartGisCsMapHost";
constexpr UINT_PTR kPresentTimerId = 1;
constexpr UINT_PTR kBlitTimerId = 2;

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
  int best_area = -1;
};

BOOL CALLBACK enum_island_proc(HWND hwnd, LPARAM lp) {
  auto* ctx = reinterpret_cast<EnumIslandCtx*>(lp);
  wchar_t cls[256] = {};
  GetClassNameW(hwnd, cls, 256);
  if (class_is_xaml_island(cls)) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > ctx->best_area) {
      ctx->best_area = area;
      ctx->found = hwnd;
    }
  }
  EnumChildWindows(hwnd, enum_island_proc, lp);
  return TRUE;
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

// Minimal CatalogCall open parser: {"op":"open","path":"..."}.
bool catalog_json_open_path(const char* json, std::string* path_out) {
  if (!json || !path_out || !std::strstr(json, "\"open\"")) {
    return false;
  }
  const char* key = std::strstr(json, "\"path\"");
  if (!key) {
    return false;
  }
  const char* colon = std::strchr(key, ':');
  if (!colon) {
    return false;
  }
  const char* q1 = std::strchr(colon, '"');
  if (!q1) {
    return false;
  }
  ++q1;
  std::string path;
  for (const char* p = q1; *p; ++p) {
    if (*p == '\\' && p[1]) {
      path.push_back(p[1]);
      ++p;
      continue;
    }
    if (*p == '"') {
      break;
    }
    path.push_back(*p);
  }
  if (path.empty()) {
    return false;
  }
  *path_out = std::move(path);
  return true;
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
    // Defer china_plp / OGR seed until first layout/paint. Loading GDAL in
    // MapSession() runs on the WinUI UI thread and contributes to hangs /
    // stowed XAML exceptions during Activate.
  }

  ~SgHost() override {
    destroy_child_hwnd();
    if (seed_thread_.joinable()) {
      seed_thread_.join();
    }
    if (session) {
      session->SetObserver(nullptr);
      session->Shutdown();
      delete session;
      session = nullptr;
    }
  }

  void OnFrameReady(uint32_t view_id, uint32_t generation) override {
    if (view_id != view_id_ || !child_hwnd_ || !IsWindow(child_hwnd_)) {
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
  void fit_document_to_client();
  void handle_catalog_json(const char* json);
  void invalidate_map();
  void ensure_document_seeded();
  void install_owner_subclass();
  void remove_owner_subclass();
  // Island-client (x,y,w,h) → screen for WS_POPUP; clamp + DPI de-dupe.
  bool map_slot_to_screen(int x, int y, int w, int h, int* sx, int* sy,
                          int* sw, int* sh);
  void apply_popup_bounds(int sx, int sy, int sw, int sh, bool force_fit);
  void apply_pointer(const content::InputEvent& ev);
  void schedule_full_redraw();
  void commit_blit_preview();
  void bind_scene3d();
  static int slot_index(content::ViewKind kind);
  static LRESULT CALLBACK owner_subclass_proc(HWND hwnd,
                                              UINT msg,
                                              WPARAM wparam,
                                              LPARAM lparam,
                                              UINT_PTR subclass_id,
                                              DWORD_PTR ref_data);

  content::MapContents* session = nullptr;
  app::MapScene document;
  app::Scene3dController scene3d_;
  mutable std::mutex document_mu_;
  bool document_seeded_ = false;
  bool seed_started_ = false;
  bool needs_extent_fit_ = false;
  int last_fit_w_ = -1;
  int last_fit_h_ = -1;
  std::thread seed_thread_;
  ViewSlot slots_[3] = {};
  HWND window_hwnd_ = nullptr;
  HWND island_hwnd_ = nullptr;
  HWND child_hwnd_ = nullptr;
  bool owner_subclassed_ = false;
  bool applying_bounds_ = false;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  // Last MapView slot in island-client physical pixels (from C# SyncLayout).
  int slot_x_ = -1;
  int slot_y_ = -1;
  int slot_w_ = -1;
  int slot_h_ = -1;
  int last_sx_ = -1;
  int last_sy_ = -1;
  int last_sw_ = -1;
  int last_sh_ = -1;
  content::ViewKind kind_ = content::ViewKind::kMapEdit;
  content::MapWidgetHostView* view_ = nullptr;
  std::string active_tool_{"view.pan"};
  bool dragging_ = false;
  int last_pointer_x_ = 0;
  int last_pointer_y_ = 0;
  mutable app::BlitFrameCache blit_;
};

int SgHost::slot_index(content::ViewKind kind) {
  return int_from_kind(kind);
}

void SgHost::attach_child_hwnd() {
  if (!window_hwnd_) {
    return;
  }
  // Do not parent a WS_CHILD under DesktopChildSiteBridge (XAML 0xc000027b),
  // and do not use a top-level WS_CHILD either — the island compositor paints
  // over Win32 siblings, so the map slot stays black. Owned WS_POPUP sits
  // above the island at screen coords matching the MapView slot.
  island_hwnd_ = resolve_island_hwnd(window_hwnd_);
  register_child_class();
  if (child_hwnd_ && !IsWindow(child_hwnd_)) {
    child_hwnd_ = nullptr;
  }
  if (!child_hwnd_) {
    last_sx_ = last_sy_ = last_sw_ = last_sh_ = -1;
    child_hwnd_ = CreateWindowExW(
        WS_EX_NOACTIVATE, kChildClass, L"",
        WS_POPUP | WS_CLIPSIBLINGS | WS_VISIBLE, 0, 0, 1, 1, window_hwnd_,
        nullptr, GetModuleHandleW(nullptr), this);
    install_owner_subclass();
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
  remove_owner_subclass();
  if (child_hwnd_) {
    const HWND doomed = child_hwnd_;
    child_hwnd_ = nullptr;
    if (IsWindow(doomed)) {
      SetWindowLongPtrW(doomed, GWLP_USERDATA, 0);
      DestroyWindow(doomed);
    }
  }
  last_sx_ = last_sy_ = last_sw_ = last_sh_ = -1;
  slot_x_ = slot_y_ = slot_w_ = slot_h_ = -1;
}

void SgHost::install_owner_subclass() {
  if (!window_hwnd_ || owner_subclassed_ || !IsWindow(window_hwnd_)) {
    return;
  }
  if (SetWindowSubclass(window_hwnd_, &owner_subclass_proc, 1,
                        reinterpret_cast<DWORD_PTR>(this))) {
    owner_subclassed_ = true;
  }
}

void SgHost::remove_owner_subclass() {
  if (!window_hwnd_ || !owner_subclassed_) {
    return;
  }
  if (IsWindow(window_hwnd_)) {
    RemoveWindowSubclass(window_hwnd_, &owner_subclass_proc, 1);
  }
  owner_subclassed_ = false;
}

LRESULT CALLBACK SgHost::owner_subclass_proc(HWND hwnd,
                                             UINT msg,
                                             WPARAM wparam,
                                             LPARAM lparam,
                                             UINT_PTR /*subclass_id*/,
                                             DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<SgHost*>(ref_data);
  if (self && self->child_hwnd_ && IsWindow(self->child_hwnd_) &&
      self->slot_w_ > 0 && self->slot_h_ > 0) {
    if (msg == WM_SIZE && wparam == SIZE_MINIMIZED) {
      ShowWindow(self->child_hwnd_, SW_HIDE);
    } else if (msg == WM_MOVE || msg == WM_SIZE || msg == WM_DISPLAYCHANGE) {
      int sx = 0;
      int sy = 0;
      int sw = 0;
      int sh = 0;
      if (self->map_slot_to_screen(self->slot_x_, self->slot_y_, self->slot_w_,
                                   self->slot_h_, &sx, &sy, &sw, &sh)) {
        self->apply_popup_bounds(sx, sy, sw, sh, false);
      }
    }
  }
  return DefSubclassProc(hwnd, msg, wparam, lparam);
}

bool SgHost::map_slot_to_screen(int x, int y, int w, int h, int* sx, int* sy,
                                int* sw, int* sh) {
  if (!sx || !sy || !sw || !sh || w < 1 || h < 1 || !window_hwnd_) {
    return false;
  }
  // Always pick the largest island — a stale tiny bridge HWND would clamp the
  // popup to a title-bar-sized rect and hide China PLP.
  HWND island = resolve_island_hwnd(window_hwnd_);
  island_hwnd_ = island;
  if (!island || !IsWindow(island)) {
    island = window_hwnd_;
  }

  RECT island_rc = {};
  GetClientRect(island, &island_rc);
  const int island_w = island_rc.right - island_rc.left;
  const int island_h = island_rc.bottom - island_rc.top;
  int lx = x;
  int ly = y;
  int lw = w;
  int lh = h;
  // Match MapHost: if physical size exceeds island, ActualWidth was likely
  // already physical and C# applied RasterizationScale twice — drop scale.
  if (island_w > 0 && island_h > 0 && (lw > island_w || lh > island_h)) {
    const double fx =
        lw > island_w ? static_cast<double>(island_w) / static_cast<double>(lw)
                      : 1.0;
    const double fy =
        lh > island_h ? static_cast<double>(island_h) / static_cast<double>(lh)
                      : 1.0;
    const double f = fx < fy ? fx : fy;
    if (f > 0.0 && f < 0.999) {
      lx = static_cast<int>(x * f + (x >= 0 ? 0.5 : -0.5));
      ly = static_cast<int>(y * f + (y >= 0 ? 0.5 : -0.5));
      lw = static_cast<int>(w * f + 0.5);
      lh = static_cast<int>(h * f + 0.5);
    }
  }
  if (island_w > 0 && island_h > 0) {
    if (lx < 0) {
      lw += lx;
      lx = 0;
    }
    if (ly < 0) {
      lh += ly;
      ly = 0;
    }
    if (lx + lw > island_w) {
      lw = island_w - lx;
    }
    if (ly + lh > island_h) {
      lh = island_h - ly;
    }
  }
  if (lw < 8 || lh < 8) {
    return false;
  }

  POINT pt = {lx, ly};
  ClientToScreen(island, &pt);
  *sx = pt.x;
  *sy = pt.y;
  *sw = lw;
  *sh = lh;
  return true;
}

void SgHost::apply_popup_bounds(int sx, int sy, int sw, int sh, bool force_fit) {
  if (!child_hwnd_ || !IsWindow(child_hwnd_) || sw < 1 || sh < 1) {
    return;
  }
  if (applying_bounds_) {
    return;
  }
  // Ignore a tiny first layout once we already have a usable slot — prevents
  // a coalesced/stale SyncLayout from pinning the popup over the title bar.
  if (!force_fit && (sw < 64 || sh < 64) && last_sw_ >= 64 && last_sh_ >= 64) {
    return;
  }
  applying_bounds_ = true;
  const bool same = last_sx_ == sx && last_sy_ == sy && last_sw_ == sw &&
                    last_sh_ == sh && IsWindowVisible(child_hwnd_);
  const bool size_changed = last_sw_ != sw || last_sh_ != sh;
  if (!same) {
    SetWindowPos(child_hwnd_, HWND_TOP, sx, sy, sw, sh,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
  last_sx_ = sx;
  last_sy_ = sy;
  last_sw_ = sw;
  last_sh_ = sh;
  if (view_) {
    view_->Resize(sw, sh, 96.f);
  }
  if (force_fit || size_changed || needs_extent_fit_) {
    fit_document_to_client();
  }
  applying_bounds_ = false;
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
  // HWND is created on first SyncLayout (stable size). Rebind if it exists.
  if (child_hwnd_) {
    attach_child_hwnd();
    if (view_) {
      view_->SetVisible(true);
    }
    start_present_timer();
  } else if (view_) {
    view_->SetVisible(true);
  }
  ensure_document_seeded();
  bind_scene3d();
}

void SgHost::bind_scene3d() {
  scene3d_.bind_map(&document);
  if (!session || view_id_ == 0) {
    return;
  }
  content::Extent2 e;
  {
    std::lock_guard<std::mutex> lock(document_mu_);
    e = document.world_extent();
  }
  if (!app::extent_looks_like_china(e)) {
    e = app::kChinaLonLatExtent;
  }
  session->SetExtent(view_id_, e);
  scene3d_.bind_contents(session, view_id_);
  scene3d_.apply_world_extent(e);
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

void SgHost::ensure_document_seeded() {
  if (document_seeded_ || seed_started_) {
    return;
  }
  seed_started_ = true;
  // OGR/GDAL open must not run on the WinUI UI thread — it blocks the
  // dispatcher and surfaces as XAML 0xc000027b / Not Responding.
  seed_thread_ = std::thread([this]() {
    app::MapScene local;
    local.seed_default();
    {
      std::lock_guard<std::mutex> lock(document_mu_);
      document = std::move(local);
      document_seeded_ = true;
      // open_path fits to 800x600; mark dirty so the next sized SyncLayout
      // refits to the live HWND (seed often finishes while the popup is 1x1).
      needs_extent_fit_ = true;
    }
    fit_document_to_client();
    invalidate_map();
  });
}

void SgHost::paint_to_dc(HDC hdc, const RECT& rc) const {
  if (!hdc) {
    return;
  }
  const int w = rc.right > 0 ? rc.right : 1;
  const int h = rc.bottom > 0 ? rc.bottom : 1;
  if (kind_ != content::ViewKind::kScene3d && blit_.in_preview() &&
      blit_.present(hdc, w, h)) {
    return;
  }
  const bool presented = present_latest_frame(hdc, rc);
  if (!presented) {
    const bool scene3d = kind_ == content::ViewKind::kScene3d;
    if (scene3d && w > 0 && h > 0) {
      scene3d_.paint(hdc, w, h);
    } else {
      const HBRUSH brush =
          CreateSolidBrush(scene3d ? RGB(18, 32, 48) : RGB(255, 255, 255));
      FillRect(hdc, &rc, brush);
      DeleteObject(brush);
    }
  } else if (kind_ == content::ViewKind::kScene3d && w > 0 && h > 0) {
    scene3d_.paint_hud(hdc, w, h);
  }
  // Overlay China city / OGR vectors + annotations (same as Views / WinUI).
  // 3D tab skips white wipe so DEM / GPU frames stay visible.
  {
    std::lock_guard<std::mutex> lock(document_mu_);
    if (document.feature_count() > 0) {
      const bool fill_bg = kind_ != content::ViewKind::kScene3d;
      document.paint(hdc, w, h, fill_bg);
    }
  }
  if (kind_ != content::ViewKind::kScene3d && w > 0 && h > 0) {
    blit_.capture(hdc, w, h);
  }
}

void SgHost::fit_document_to_client() {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return;
  }
  ensure_document_seeded();
  RECT rc = {};
  GetClientRect(child_hwnd_, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w < 8 || h < 8) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(document_mu_);
    if (document.feature_count() == 0) {
      return;
    }
    if (!needs_extent_fit_ && last_fit_w_ == w && last_fit_h_ == h) {
      return;
    }
    document.fit_extent(w, h);
    last_fit_w_ = w;
    last_fit_h_ = h;
    needs_extent_fit_ = false;
  }
  if (kind_ == content::ViewKind::kScene3d) {
    bind_scene3d();
  }
  invalidate_map();
}

void SgHost::paint_child() const {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return;
  }
  // Seed often finishes while the popup is still 1x1; refit on first real paint.
  const_cast<SgHost*>(this)->fit_document_to_client();
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(child_hwnd_, &ps);
  RECT rc;
  GetClientRect(child_hwnd_, &rc);
  paint_to_dc(hdc, rc);
  EndPaint(child_hwnd_, &ps);
}

void SgHost::invalidate_map() {
  if (child_hwnd_ && IsWindow(child_hwnd_)) {
    InvalidateRect(child_hwnd_, nullptr, FALSE);
  }
}

void SgHost::schedule_full_redraw() {
  if (!child_hwnd_ || !IsWindow(child_hwnd_)) {
    return;
  }
  KillTimer(child_hwnd_, kBlitTimerId);
  SetTimer(child_hwnd_, kBlitTimerId,
           static_cast<UINT>(tool::kBlitDebounceMs), nullptr);
}

void SgHost::commit_blit_preview() {
  blit_.end_preview();
  invalidate_map();
}

void SgHost::apply_pointer(const content::InputEvent& ev) {
  RECT rc = {};
  int w = last_sw_;
  int h = last_sh_;
  if (child_hwnd_ && IsWindow(child_hwnd_)) {
    GetClientRect(child_hwnd_, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
  }
  if (w < 8 || h < 8) {
    return;
  }

  if (kind_ == content::ViewKind::kScene3d) {
    if (ev.kind == content::InputEvent::Kind::kWheel) {
      scene3d_.apply_wheel_at(ev.x_px, ev.y_px, ev.wheel, w, h);
      invalidate_map();
      schedule_full_redraw();
      return;
    }
    const bool is_pan = active_tool_.empty() || active_tool_ == "view.pan" ||
                        active_tool_ == "view3d.trackball";
    if (ev.kind == content::InputEvent::Kind::kLDown) {
      dragging_ = true;
      last_pointer_x_ = ev.x_px;
      last_pointer_y_ = ev.y_px;
      if (child_hwnd_) {
        SetCapture(child_hwnd_);
      }
      return;
    }
    if (ev.kind == content::InputEvent::Kind::kLUp ||
        ev.kind == content::InputEvent::Kind::kRUp) {
      dragging_ = false;
      ReleaseCapture();
      return;
    }
    if (ev.kind == content::InputEvent::Kind::kMouseMove && dragging_ &&
        is_pan) {
      const int dx = ev.x_px - last_pointer_x_;
      const int dy = ev.y_px - last_pointer_y_;
      scene3d_.apply_pan(dx, dy);
      last_pointer_x_ = ev.x_px;
      last_pointer_y_ = ev.y_px;
      invalidate_map();
      schedule_full_redraw();
    }
    return;
  }

  std::lock_guard<std::mutex> lock(document_mu_);
  if (document.feature_count() == 0) {
    return;
  }

  if (ev.kind == content::InputEvent::Kind::kWheel) {
    const double factor = ev.wheel > 0 ? 1.15 : (1.0 / 1.15);
    blit_.begin_zoom(w, h, ev.x_px, ev.y_px, factor);
    document.apply_zoom_at(ev.x_px, ev.y_px, factor);
    invalidate_map();
    schedule_full_redraw();
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
    if (is_select) {
      document.hit_test(ev.x_px, ev.y_px, w, h);
    } else if (is_zoom_in) {
      blit_.begin_zoom(w, h, ev.x_px, ev.y_px, 1.15);
      document.apply_zoom_at(ev.x_px, ev.y_px, 1.15);
      schedule_full_redraw();
    } else if (is_zoom_out) {
      blit_.begin_zoom(w, h, ev.x_px, ev.y_px, 1.0 / 1.15);
      document.apply_zoom_at(ev.x_px, ev.y_px, 1.0 / 1.15);
      schedule_full_redraw();
    }
    invalidate_map();
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
    document.apply_pan(dx, dy);
    last_pointer_x_ = ev.x_px;
    last_pointer_y_ = ev.y_px;
    invalidate_map();
    schedule_full_redraw();
  }
}

void SgHost::handle_catalog_json(const char* json) {
  std::string path;
  if (!catalog_json_open_path(json, &path)) {
    return;
  }
  seed_started_ = true;
  if (seed_thread_.joinable()) {
    seed_thread_.join();
  }
  seed_thread_ = std::thread([this, path]() {
    app::MapScene local;
    local.open_path(path);
    {
      std::lock_guard<std::mutex> lock(document_mu_);
      document = std::move(local);
      document_seeded_ = true;
      needs_extent_fit_ = true;
    }
    fit_document_to_client();
    invalidate_map();
  });
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

  if (msg == WM_TIMER && wparam == kBlitTimerId && self) {
    KillTimer(hwnd, kBlitTimerId);
    self->commit_blit_preview();
    return 0;
  }

  if (msg == WM_TIMER && wparam == kPresentTimerId) {
    if (!self || !self->view_ || self->child_hwnd_ != hwnd || !IsWindow(hwnd)) {
      return 0;
    }
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
  // Create the map child only after the XAML island has a stable client
  // size (first SyncLayout). Attaching a WS_CHILD to DesktopChildSiteBridge
  // during Activate races CoreMessaging and yields 0xc000027b.
}

void sg_host_sync_layout(SgHost* host, int x, int y, int w, int h, float dpi) {
  if (!host || !host->window_hwnd_ || w < 1 || h < 1) {
    return;
  }
  if (!host->child_hwnd_) {
    host->attach_child_hwnd();
    if (!host->child_hwnd_) {
      return;
    }
    host->start_present_timer();
    host->ensure_document_seeded();
  }
  if (!IsWindow(host->child_hwnd_)) {
    host->child_hwnd_ = nullptr;
    return;
  }
  host->slot_x_ = x;
  host->slot_y_ = y;
  host->slot_w_ = w;
  host->slot_h_ = h;
  int sx = 0;
  int sy = 0;
  int sw = 0;
  int sh = 0;
  if (!host->map_slot_to_screen(x, y, w, h, &sx, &sy, &sw, &sh)) {
    return;
  }
  host->apply_popup_bounds(sx, sy, sw, sh, false);
  if (host->view_) {
    const float scale = dpi > 1.f ? dpi : 96.f;
    host->view_->Resize(sw, sh, scale);
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
  if (!host) {
    return 0;
  }
  {
    std::lock_guard<std::mutex> lock(host->document_mu_);
    if (host->document.feature_count() >= 3) {
      return 1;
    }
  }
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
  if (!host || !json || !json[0]) {
    return;
  }
  // Open China PLP / OGR into the host-side MapScene overlay first.
  host->handle_catalog_json(json);
  if (host->session) {
    host->session->CatalogCall(json);
  }
}

void sg_host_activate_tool(SgHost* host, const char* tool_id) {
  if (!host || !tool_id || !tool_id[0]) {
    return;
  }
  host->active_tool_ = tool_id;
  if (host->active_tool_ == "selection.clear") {
    std::lock_guard<std::mutex> lock(host->document_mu_);
    host->document.clear_selection();
    host->invalidate_map();
  }
  if (!host->session || host->view_id_ == 0) {
    return;
  }
  host->session->ActivateTool(host->view_id_, tool_id);
}

void sg_host_dispatch_pointer(SgHost* host,
                              int kind,
                              int x_px,
                              int y_px,
                              int wheel) {
  if (!host) {
    return;
  }
  content::InputEvent ev{};
  ev.kind = static_cast<content::InputEvent::Kind>(kind);
  ev.x_px = x_px;
  ev.y_px = y_px;
  ev.wheel = wheel;
  host->apply_pointer(ev);
  if (host->session && host->view_id_ != 0) {
    host->session->Dispatch(host->view_id_, ev);
  }
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
