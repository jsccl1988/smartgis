// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map_viewport.h"

#include <cstdint>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include "render/skia/canvas.h"

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

  if (try_content_map_view()) {
    mode_ = AttachMode::kContentMapView;
    status_ = L"content::MapWidgetHostView";
    return true;
  }
  // Only the Map Edit pane tries leftover OOP / LoadLibrary hang.
  if (role_ == Role::kMapEdit) {
    if (try_oop_render()) {
      mode_ = AttachMode::kOopRender;
      status_ = L"OOP SmartGisRender.exe";
      paint_child_placeholder();
      return true;
    }
    if (try_local_device()) {
      mode_ = AttachMode::kLocalDevice;
      status_ = L"SmtRenderDevice::Init (LoadLibrary)";
      return true;
    }
  }
  mode_ = AttachMode::kPlaceholder;
  if (role_ == Role::kScene3d) {
    status_ = L"3D placeholder (no scene attach)";
  } else if (role_ == Role::kMapData) {
    status_ = L"Datasource browse (placeholder)";
  } else {
    status_ = L"Placeholder map (no render exe / device DLL)";
  }
  paint_child_placeholder();
  return false;
}

void MapViewport::detach() {
#ifdef SMT_HAS_CONTENT_MAP_SESSION
  if (owns_session_ && session_) {
    session_->Shutdown();
    delete session_;
  }
#endif
  session_ = nullptr;
  owns_session_ = false;
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
  view_id_ = 0;
  mode_ = AttachMode::kNone;
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
    view->Resize(width_px, height_px, 96.0f);
  }
#else
  (void)width_px;
  (void)height_px;
#endif
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
  if (content::MapWidgetHostView* view =
          session_->AttachSurface(view_id_, content::PresentMode::kChildHwnd)) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = native_view();
    view->Create(params, content::MapWidgetHostView::Preferences{});
    RECT rc = {};
    GetClientRect(native_view(), &rc);
    view->Resize(rc.right, rc.bottom, 96.0f);
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

bool MapViewport::try_local_device() {
  const wchar_t* names[] = {
      L"SmtGdiRenderDeviceD.dll",
      L"SmtGdiSimpleRenderDeviceD.dll",
      L"SmtGdiRenderDevice.dll",
      L"SmtGdiSimpleRenderDevice.dll",
      L"SmtGdiSRenderDevice.dll",
      L"SmtGLRenderDeviceD.dll",
      L"SmtGLRenderDevice.dll",
      L"SmtRenderD.dll",
      L"SmtRender.dll",
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
  InvalidateRect(hwnd, nullptr, TRUE);
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
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    render::skia::Canvas canvas(hdc, rc.right, rc.bottom);
    canvas.fill_rect(0, 0, rc.right, rc.bottom,
                     render::skia::color_rgb(27, 58, 75));
    const wchar_t* text = L"Map viewport";
    if (self && self->status_) {
      text = self->status_;
    }
    canvas.draw_text(16, 16, L"SmartGIS map HWND",
                     render::skia::color_rgb(220, 230, 240));
    canvas.draw_text(16, 40, text, render::skia::color_rgb(160, 200, 180));
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
