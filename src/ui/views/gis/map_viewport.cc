// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map_viewport.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include "render/rhi/rhi.h"
#include "render/skia/canvas.h"
#include "ui/views/dpi.h"
#include "ui/views/widget.h"

#if defined(__has_include)
#if __has_include("content/public/map_contents.h")
#include "content/public/map_contents.h"
#define SMT_HAS_CONTENT_MAP_SESSION 1
#endif
#if __has_include("content/public/view_host.h")
#include "content/public/view_host.h"
#define SMT_HAS_VIEW_HOST 1
#endif
#if __has_include("ui/shell/map_session.h")
#include "ui/shell/map_session.h"
#define SMT_HAS_UI_SHELL 1
#endif
#endif

namespace ui {
namespace views {
namespace {

const wchar_t kMapClass[] = L"SmartGisMapViewport";

using CreateRenderDeviceFn = int (*)(HINSTANCE, void*&);

struct DeviceVtable {
  void* dtor;
  int (*Init)(void* self, HWND hwnd, const char* logname);
  int (*Destroy)(void* self);
  int (*Release)(void* self);
  int (*Resize)(void* self, int orgx, int orgy, int cx, int cy);
};

struct DeviceObj {
  DeviceVtable* vtbl;
};

bool file_exists(const wchar_t* path) {
  const DWORD attr = GetFileAttributesW(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

void exe_dir(wchar_t* out, size_t cap) {
  GetModuleFileNameW(nullptr, out, static_cast<DWORD>(cap));
  wchar_t* slash = wcsrchr(out, L'\\');
  if (slash) {
    slash[1] = 0;
  }
}

HMODULE load_first(const wchar_t* const* names) {
  for (size_t i = 0; names[i]; ++i) {
    if (HMODULE already = GetModuleHandleW(names[i])) {
      return already;
    }
    if (HMODULE mod = LoadLibraryW(names[i])) {
      return mod;
    }
  }
  return nullptr;
}

bool init_device_seh(void* device, HWND hwnd) {
  auto* obj = static_cast<DeviceObj*>(device);
  if (!obj || !obj->vtbl || !obj->vtbl->Init) {
    return false;
  }
  __try {
    return obj->vtbl->Init(obj, hwnd, "SmartGisViews") == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// Same Win32 → InputEvent mapping as leftover dispatch_chrome_message.
bool route_view_host_input(content::ViewHost* host,
                           HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam) {
#ifdef SMT_HAS_VIEW_HOST
  if (!host) {
    return false;
  }
  content::InputEvent e{};
  switch (message) {
    case WM_MOUSEMOVE:
      e.kind = content::InputEvent::Kind::kMouseMove;
      break;
    case WM_LBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kLDown;
      break;
    case WM_LBUTTONUP:
      e.kind = content::InputEvent::Kind::kLUp;
      break;
    case WM_LBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kLDClick;
      break;
    case WM_RBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      break;
    case WM_RBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      break;
    case WM_RBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kRDClick;
      break;
    case WM_MOUSEWHEEL: {
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam));
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      return host->dispatch_input(e);
    }
    case WM_KEYDOWN:
      e.kind = content::InputEvent::Kind::kKeyDown;
      e.key = static_cast<uint32_t>(wparam);
      return host->dispatch_input(e);
    default:
      return false;
  }
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
  e.flags = static_cast<uint32_t>(wparam);
  return host->dispatch_input(e);
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)lparam;
  return false;
#endif
}

}  // namespace

MapViewport::MapViewport() {
  set_preferred_size({400, 300});
}

MapViewport::~MapViewport() {
  detach();
}

void MapViewport::set_role(Role role) {
  role_ = role;
}

void MapViewport::set_view_host(content::ViewHost* host) {
  view_host_ = host;
}

void MapViewport::set_map_contents(content::MapContents* session) {
  if (owns_session_ && session_ && session_ != session) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
    session_->Shutdown();
    delete session_;
#endif
  }
  session_ = session;
  owns_session_ = false;
}

HWND MapViewport::create_native_view(HWND parent) {
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MapViewport::child_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = kMapClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  HWND hwnd = CreateWindowExW(0, kMapClass, L"",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                              0, 0, 1, 1, parent, nullptr,
                              GetModuleHandleW(nullptr), this);
  return hwnd;
}

void MapViewport::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  // Native child paints the map. Chrome draws a caption strip above if
  // the host left a gap; here the child fills the view.
}

bool MapViewport::attach() {
  if (!native_view()) {
    realize_native();
  }
  if (!native_view()) {
    mode_ = AttachMode::kPlaceholder;
    status_ = L"No map HWND";
    return false;
  }
  SetWindowLongPtrW(native_view(), GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(this));

  // Scene3d: prefer FlyCube for live orbit camera when requested (or when
  // content hang is unavailable). Default self-test keeps content-first to
  // avoid FlyCube CRT teardown issues on machines without a stable adapter.
  const bool prefer_flycube_3d = []() {
    if (const char* env = std::getenv("SMT_PREFER_FLYCUBE_3D")) {
      return env[0] == '1' && env[1] == '\0';
    }
    return false;
  }();
  if (role_ == Role::kScene3d && prefer_flycube_3d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"3D FlyCube RHI present (DX12)";
      start_present_timer();
      return true;
    }
  }

  if (try_content_map_view()) {
    mode_ = AttachMode::kContentMapView;
    status_ = (role_ == Role::kScene3d)
                  ? L"content::MapWidgetHostView (3D)"
                  : L"content::MapWidgetHostView";
    start_present_timer();
    paint_child_placeholder();
    return true;
  }
  // Map Edit: leftover OOP / FlyCube / LoadLibrary. Scene3d: FlyCube fallback.
  if (role_ == Role::kMapEdit) {
    if (try_oop_render()) {
      mode_ = AttachMode::kOopRender;
      status_ = L"OOP SmartGisRender.exe";
      paint_child_placeholder();
      return true;
    }
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"FlyCube RHI present (DX12)";
      return true;
    }
    if (try_local_device()) {
      mode_ = AttachMode::kLocalDevice;
      status_ = L"CreateRenderDevice (LoadLibrary)";
      return true;
    }
  } else if (role_ == Role::kScene3d) {
    if (try_flycube_device()) {
      mode_ = AttachMode::kFlyCube;
      status_ = L"3D FlyCube RHI present (DX12)";
      start_present_timer();
      return true;
    }
  }
  mode_ = AttachMode::kPlaceholder;
  if (role_ == Role::kScene3d) {
    status_ = L"3D placeholder (no scene device)";
  } else if (role_ == Role::kMapData) {
    status_ = L"Datasource browse (placeholder)";
  } else {
    status_ = L"Placeholder map (no render exe / device DLL)";
  }
  paint_child_placeholder();
  // HWND is live; callers treat placeholder as a successful UI hang.
  return native_view() != nullptr;
}

void MapViewport::detach() {
  stop_present_timer();
  release_backbuffer();
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (owns_session_ && session_) {
    session_->Shutdown();
    delete session_;
  }
#endif
  session_ = nullptr;
  owns_session_ = false;
  painted_generation_ = 0;
  if (HWND hwnd = native_view()) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  }
  if (render_process_) {
    TerminateProcess(render_process_, 0);
    CloseHandle(render_process_);
    render_process_ = nullptr;
  }
  if (render_job_) {
    CloseHandle(render_job_);
    render_job_ = nullptr;
  }
  if (local_device_) {
    auto* obj = static_cast<DeviceObj*>(local_device_);
    if (obj->vtbl && obj->vtbl->Release) {
      obj->vtbl->Release(obj);
    }
    local_device_ = nullptr;
  }
  local_module_ = nullptr;
  release_rhi_device();
  view_id_ = 0;
  mode_ = AttachMode::kNone;
}

void MapViewport::set_overlay_paint(OverlayPaint fn) {
  overlay_paint_ = std::move(fn);
}

void MapViewport::set_gpu_present(GpuPresentFn fn) {
  gpu_present_ = std::move(fn);
}

void MapViewport::invalidate_native() {
  if (HWND hwnd = native_view()) {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
}

bool MapViewport::wait_ready(uint32_t timeout_ms) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (session_ && view_id_ != 0) {
    return session_->WaitFrameReady(view_id_, timeout_ms);
  }
#endif
  return native_view() && IsWindow(native_view());
}

void MapViewport::resize_host_surface(int width_px, int height_px) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (!session_ || view_id_ == 0 || width_px <= 0 || height_px <= 0) {
    return;
  }
  if (content::MapWidgetHostView* view = session_->HostView(view_id_)) {
    view->Resize(width_px, height_px, surface_dpi());
  }
#else
  (void)width_px;
  (void)height_px;
#endif
}

float MapViewport::surface_dpi() const {
  if (widget()) {
    return static_cast<float>(widget()->dpi());
  }
  return static_cast<float>(dpi_for_hwnd(native_view()));
}

void MapViewport::on_device_scale_factor_changed(float old_scale,
                                               float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  if (HWND hwnd = native_view()) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    resize_host_surface(rc.right - rc.left, rc.bottom - rc.top);
  }
}

bool MapViewport::try_content_map_view() {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (!session_) {
    session_ = content::MapContents::Create();
    if (!session_) {
      return false;
    }
    owns_session_ = true;
    if (!session_->StartRenderProcess()) {
      session_->Shutdown();
      delete session_;
      session_ = nullptr;
      owns_session_ = false;
      return false;
    }
  }
  content::ViewKind kind = content::ViewKind::kMapEdit;
  if (role_ == Role::kMapData) {
    kind = content::ViewKind::kMapData;
  } else if (role_ == Role::kScene3d) {
    kind = content::ViewKind::kScene3d;
  }
  view_id_ = session_->OpenView(kind);
  if (view_id_ == 0) {
    return false;
  }
  // Software DIB: GPU publishes shared pixels; this HWND presents Latest().
  // kChildHwnd is reserved for a future in-GPU child window; both fall back
  // to create_dib in PresentTarget::resize today.
  if (content::MapWidgetHostView* view = session_->AttachSurface(
          view_id_, content::PresentMode::kSoftwareDib)) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = native_view();
    view->Create(params, content::MapWidgetHostView::Preferences{});
    RECT rc = {};
    GetClientRect(native_view(), &rc);
    const int w = rc.right > 0 ? rc.right : 64;
    const int h = rc.bottom > 0 ? rc.bottom : 64;
    view->Resize(w, h, surface_dpi());
  }
  return true;
#else
  return false;
#endif
}

bool MapViewport::try_oop_render() {
#ifdef SMT_HAS_UI_SHELL
  if (ui::shell::IMapSession* session = ui::shell::create_map_session()) {
    if (session->start_render_process()) {
      session->open_view(ui::shell::ViewKind::kMapEdit);
      session->attach_surface(1, ui::shell::PresentMode::kChildHwnd);
      return true;
    }
  }
#endif

  wchar_t dir[MAX_PATH] = {};
  exe_dir(dir, MAX_PATH);
  wchar_t path[MAX_PATH] = {};
  lstrcpynW(path, dir, MAX_PATH);
  lstrcpynW(path + lstrlenW(path), L"SmartGisRender.exe",
            MAX_PATH - lstrlenW(path));
  if (!file_exists(path)) {
    return false;
  }

  const DWORD pid = GetCurrentProcessId();
  wchar_t session[64] = {};
  swprintf_s(session, L"%u-%lu", pid, GetTickCount());

  wchar_t cmd[1024] = {};
  swprintf_s(cmd,
             L"\"%s\" --parent-pid=%u --pipe=smartgis-host-%u --session=%s "
             L"--map-hwnd=%llu --present=child_hwnd",
             path, pid, pid, session,
             static_cast<unsigned long long>(
                 reinterpret_cast<uintptr_t>(native_view())));

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(path, cmd, nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                      nullptr, dir, &si, &pi)) {
    return false;
  }

  render_job_ = CreateJobObjectW(nullptr, nullptr);
  if (render_job_) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim = {};
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(render_job_, JobObjectExtendedLimitInformation,
                            &lim, sizeof(lim));
    AssignProcessToJobObject(render_job_, pi.hProcess);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  render_process_ = pi.hProcess;
  return true;
}

void MapViewport::release_rhi_device() {
  if (!rhi_device_) {
    return;
  }
  auto* device = static_cast<render::rhi::Device*>(rhi_device_);
  device->shutdown();
  // FlyCube CRT / allocator can heap-corrupt on operator delete after a live
  // DX12 session; leak the facade (matches rhi_test / unified_draw_test).
  rhi_device_ = nullptr;
}

bool MapViewport::try_flycube_device() {
  // Opt-out: MFC / leftover GDI still required for some hosts.
  if (const char* prefer = std::getenv("SMT_PREFER_GDI_DEVICE")) {
    if (prefer[0] == '1' && prefer[1] == '\0') {
      return false;
    }
  }
#ifndef SMT_HAS_FLYCUBE
  return false;
#else
  HWND hwnd = native_view();
  if (!hwnd) {
    return false;
  }
  release_rhi_device();
  render::rhi::Device* device =
      render::rhi::create_device(render::rhi::preferred_gpu_backend());
  if (!device) {
    return false;
  }
  RECT rc = {};
  GetClientRect(hwnd, &rc);
  render::rhi::DeviceDesc desc;
  desc.native_window = hwnd;
  desc.width = rc.right > 0 ? static_cast<uint32_t>(rc.right) : 1;
  desc.height = rc.bottom > 0 ? static_cast<uint32_t>(rc.bottom) : 1;
  if (!device->initialize(desc)) {
    device->shutdown();
    // Do not delete — FlyCube teardown has corrupted the process heap.
    return false;
  }
  // One clear+present proves the swapchain is live; GIS draws land later via
  // LeftoverRecorder / GpuScene on the same HWND session.
  render::rhi::CommandList* list = device->create_command_list();
  if (list) {
    render::rhi::RenderPassDesc pass;
    pass.clear_r = 0.05f;
    pass.clear_g = 0.12f;
    pass.clear_b = 0.18f;
    pass.clear_a = 1.f;
    pass.width = desc.width;
    pass.height = desc.height;
    list->begin_render_pass(pass);
    list->set_viewport(0, 0, static_cast<float>(desc.width),
                       static_cast<float>(desc.height), 0, 1);
    list->end_render_pass();
    list->close();
    device->execute(list);
    device->destroy_command_list(list);
  }
  device->present();
  rhi_device_ = device;
  return true;
#endif
}

bool MapViewport::try_local_device() {
  // Device DLLs that export CreateRenderDevice (current dll_stem + _d).
  const wchar_t* names[] = {
      L"render_gdi_simple_d.dll",
      L"render_gdi_d.dll",
      L"render_gl_d.dll",
      L"render_gdi_simple.dll",
      L"render_gdi.dll",
      L"render_gl.dll",
      nullptr,
  };
  local_module_ = load_first(names);
  if (!local_module_) {
    return false;
  }
  auto create = reinterpret_cast<CreateRenderDeviceFn>(
      GetProcAddress(local_module_, "CreateRenderDevice"));
  if (!create) {
    return false;
  }
  void* device = nullptr;
  if (create(local_module_, device) != 0 || !device) {
    return false;
  }
  if (!init_device_seh(device, native_view())) {
    return false;
  }
  local_device_ = device;
  RECT rc = {};
  GetClientRect(native_view(), &rc);
  auto* obj = static_cast<DeviceObj*>(device);
  if (obj->vtbl && obj->vtbl->Resize && rc.right > 0 && rc.bottom > 0) {
    obj->vtbl->Resize(obj, 0, 0, rc.right, rc.bottom);
  }
  return true;
}

void MapViewport::paint_child_placeholder() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  // Do not erase: TRUE would flash the class brush / default clear between
  // present timer ticks and the composited BitBlt.
  InvalidateRect(hwnd, nullptr, FALSE);
}

void MapViewport::release_backbuffer() {
  if (back_dc_) {
    if (back_old_) {
      SelectObject(back_dc_, back_old_);
      back_old_ = nullptr;
    }
    DeleteDC(back_dc_);
    back_dc_ = nullptr;
  }
  if (back_dib_) {
    DeleteObject(back_dib_);
    back_dib_ = nullptr;
  }
  back_w_ = 0;
  back_h_ = 0;
}

bool MapViewport::ensure_backbuffer(int width_px, int height_px) {
  if (width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (back_dc_ && back_dib_ && back_w_ == width_px && back_h_ == height_px) {
    return true;
  }
  release_backbuffer();

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(dib);
    return false;
  }
  HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, dib));
  back_dc_ = mem;
  back_dib_ = dib;
  back_old_ = old;
  back_w_ = width_px;
  back_h_ = height_px;
  // Size change discards the last composited frame; require a fresh present
  // (or placeholder) before BitBlt so we never show an empty DIB.
  painted_generation_ = 0;
  RECT fill = {0, 0, width_px, height_px};
  HBRUSH brush = CreateSolidBrush(RGB(27, 58, 75));
  FillRect(mem, &fill, brush);
  DeleteObject(brush);
  return true;
}

void MapViewport::paint_map_content(HDC target, const RECT& client_rc) {
  if (!target) {
    return;
  }
  bool presented = false;
  if (mode_ == AttachMode::kContentMapView) {
    presented = present_latest_frame(target, client_rc);
  }
  if (!presented && painted_generation_ > 0) {
    // Keep the last composited backbuffer; re-running overlay on top of a
    // frame that already includes vectors would stack strokes.
    return;
  }
  // Keep the previous backbuffer pixels when present briefly fails so the
  // viewport does not flash the teal placeholder between GPU generations.
  if (!presented) {
    // Pure GDI placeholder — avoid Skia Canvas on the retained mem DC (its
    // per-call BitBlt + DIB teardown has corrupted the process heap before).
    RECT fill = {0, 0, client_rc.right, client_rc.bottom};
    HBRUSH brush = CreateSolidBrush(RGB(27, 58, 75));
    FillRect(target, &fill, brush);
    DeleteObject(brush);
    SetBkMode(target, TRANSPARENT);
    SetTextColor(target, RGB(220, 230, 240));
    const wchar_t* title = L"SmartGIS map HWND";
    if (role_ == Role::kScene3d) {
      title = L"SmartGIS 3D HWND";
    } else if (role_ == Role::kMapData) {
      title = L"SmartGIS data HWND";
    }
    TextOutW(target, 16, 16, title, lstrlenW(title));
    SetTextColor(target, RGB(160, 200, 180));
    const wchar_t* text = status_ ? status_ : L"Map viewport";
    TextOutW(target, 16, 40, text, lstrlenW(text));
  }
  if (overlay_paint_) {
    overlay_paint_(target, client_rc);
  }
}

void MapViewport::start_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  SetTimer(hwnd, kPresentTimerId, 33, nullptr);
}

void MapViewport::stop_present_timer() {
  HWND hwnd = native_view();
  if (!hwnd) {
    return;
  }
  KillTimer(hwnd, kPresentTimerId);
}

bool MapViewport::present_latest_frame(HDC hdc, const RECT& client_rc) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (!hdc || !session_ || view_id_ == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session_->HostView(view_id_);
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  if (!surface.nt_handle || surface.generation == 0 || surface.width_px == 0 ||
      surface.height_px == 0) {
    return false;
  }
  // Cap blit size to avoid pathological maps / bad IPC metadata.
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
  // Never read past the mapped region (bad IPC size metadata corrupts heap).
  MEMORY_BASIC_INFORMATION mbi = {};
  SIZE_T mapped = 0;
  if (VirtualQuery(bits, &mbi, sizeof(mbi)) != 0) {
    mapped = mbi.RegionSize;
  }
  if (mapped == 0 || mapped < bytes) {
    UnmapViewOfFile(bits);
    return false;
  }
  // Copy out of the shared mapping before StretchDIBits so a concurrent GPU
  // resize cannot invalidate the source mid-blit.
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
#else
  (void)hdc;
  (void)client_rc;
  return false;
#endif
}

LRESULT CALLBACK MapViewport::child_wnd_proc(HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* self =
      reinterpret_cast<MapViewport*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_TIMER && wparam == kPresentTimerId) {
    if (self && self->mode_ == AttachMode::kContentMapView) {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
      if (self->session_ && self->view_id_ != 0) {
        if (content::MapWidgetHostView* view =
                self->session_->HostView(self->view_id_)) {
          const content::SharedSurface surface = view->Latest();
          if (surface.generation != 0 &&
              surface.generation != self->painted_generation_) {
            InvalidateRect(hwnd, nullptr, FALSE);
          }
        }
      }
#endif
    } else if (self && self->mode_ == AttachMode::kFlyCube &&
               self->role_ == Role::kScene3d && IsWindowVisible(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  }
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int width_px = rc.right > 0 ? rc.right : 0;
    const int height_px = rc.bottom > 0 ? rc.bottom : 0;
    // Scene3d + FlyCube: GPU presents to the HWND swapchain; HUD is light
    // GDI text only (no full-frame StretchDIBits race).
    if (self && self->role_ == Role::kScene3d &&
        self->mode_ == AttachMode::kFlyCube && self->rhi_device_ &&
        self->gpu_present_) {
      const uint32_t w = width_px > 0 ? static_cast<uint32_t>(width_px) : 1;
      const uint32_t h = height_px > 0 ? static_cast<uint32_t>(height_px) : 1;
      self->gpu_present_(self->rhi_device_, w, h);
      if (self->overlay_paint_) {
        self->overlay_paint_(hdc, rc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    // Map / 3D placeholder: composite present + vector overlay offscreen,
    // then one BitBlt so the user never sees a half-drawn frame.
    if (self && width_px > 0 && height_px > 0 &&
        self->ensure_backbuffer(width_px, height_px)) {
      self->paint_map_content(self->back_dc_, rc);
      BitBlt(hdc, 0, 0, width_px, height_px, self->back_dc_, 0, 0, SRCCOPY);
    } else if (self) {
      self->paint_map_content(hdc, rc);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_SIZE && self) {
    const int cx = static_cast<int>(LOWORD(lparam));
    const int cy = static_cast<int>(HIWORD(lparam));
    self->resize_host_surface(cx, cy);
    if (self->local_device_) {
      auto* obj = static_cast<DeviceObj*>(self->local_device_);
      if (obj->vtbl && obj->vtbl->Resize) {
        obj->vtbl->Resize(obj, 0, 0, cx, cy);
      }
    }
    if (self->rhi_device_ && cx > 0 && cy > 0) {
      auto* device = static_cast<render::rhi::Device*>(self->rhi_device_);
      render::rhi::DeviceDesc desc;
      desc.native_window = hwnd;
      desc.width = static_cast<uint32_t>(cx);
      desc.height = static_cast<uint32_t>(cy);
      device->initialize(desc);
    }
  }
  if (self && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN)) {
    SetFocus(hwnd);
  }
  if (self &&
      route_view_host_input(self->view_host_, hwnd, msg, wparam, lparam)) {
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
