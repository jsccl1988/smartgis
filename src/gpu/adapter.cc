// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/gpu.h"

#include <cstdio>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gpu {
namespace detail {
namespace {

using CreateRenderDeviceFn = int (*)(HINSTANCE, void**);

const wchar_t kWndClass[] = L"SmartGisGpuSurface";

LRESULT CALLBACK surface_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

std::wstring exe_dir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) {
    *slash = 0;
  }
  return path;
}

bool load_one(const wchar_t* stem, bool debug, std::vector<HMODULE>* loaded) {
  std::wstring path = exe_dir() + L"\\" + stem;
  if (debug) {
    path += L"_d";
  }
  path += L".dll";
  HMODULE m = LoadLibraryW(path.c_str());
  if (!m && debug) {
    std::wstring release = exe_dir() + L"\\" + stem + L".dll";
    m = LoadLibraryW(release.c_str());
  }
  if (m) {
    loaded->push_back(m);
    return true;
  }
  return false;
}

// Matches the leading vtable of SmtRenderDevice (virtual dtor + Init).
class LegacyDevice {
 public:
  virtual ~LegacyDevice() = default;
  virtual int Init(HWND hwnd, const char* logname) = 0;
};

}  // namespace

class AdapterImpl final : public Adapter {
 public:
  ~AdapterImpl() override {
    if (hwnd_) {
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
    for (HMODULE m : dlls_) {
      FreeLibrary(m);
    }
  }

  bool load_legacy_dlls() override {
    const bool debug =
#ifdef _DEBUG
        true;
#else
        false;
#endif
    // Platform layer stems (dll reorg). Debug → stem_d.dll via load_one.
    const wchar_t* required[] = {
        L"base",
        L"sdb",
        L"algorithm",
        L"render",
    };
    // Optional: UI chrome, leftover engines/tools, plugins.
    const wchar_t* optional[] = {
        L"ui_legacy",
        L"legacy_render",
        L"legacy_tool",
        L"plugin",
    };
    bool ok = true;
    for (const wchar_t* s : required) {
      if (!load_one(s, debug, &dlls_)) {
        ok = false;
      }
    }
    for (const wchar_t* s : optional) {
      load_one(s, debug, &dlls_);
    }

    const std::wstring log_dir = exe_dir() + L"\\log";
    CreateDirectoryW(log_dir.c_str(), nullptr);

    HMODULE gdi = GetModuleHandleW(debug ? L"legacy_render_d.dll"
                                         : L"legacy_render.dll");
    if (!gdi && !dlls_.empty()) {
      gdi = GetModuleHandleW(debug ? L"legacy_render_d.dll"
                                   : L"legacy_render.dll");
    }
    if (gdi) {
      auto create = reinterpret_cast<CreateRenderDeviceFn>(
          GetProcAddress(gdi, "CreateGdiSimpleRenderDevice"));
      if (!create) {
        create = reinterpret_cast<CreateRenderDeviceFn>(
            GetProcAddress(gdi, "CreateRenderDevice"));
      }
      if (create) {
        void* dev = nullptr;
        if (create(gdi, &dev) == 0 && dev) {
          device_ = dev;
        }
      }
    }
    return ok || device_ != nullptr || !dlls_.empty();
  }

  bool bind_view(uint32_t view_id, void* legacy_map) override {
    (void)view_id;
    map_ = legacy_map;
    return true;
  }

  void* render_device(uint32_t view_id) override {
    (void)view_id;
    return device_;
  }

  bool init_hidden_hwnd(int width_px, int height_px) override {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = surface_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    if (width_px < 1) {
      width_px = 64;
    }
    if (height_px < 1) {
      height_px = 64;
    }
    hwnd_ = CreateWindowExW(WS_EX_NOACTIVATE, kWndClass, L"SmartGisGpu",
                            WS_POPUP, -32000, -32000, width_px, height_px,
                            nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) {
      return false;
    }
    ShowWindow(hwnd_, SW_HIDE);
    if (device_) {
      try {
        static_cast<LegacyDevice*>(device_)->Init(hwnd_, "gpu_host");
      } catch (...) {
      }
    }
    return true;
  }

  void* hwnd() const override { return hwnd_; }

 private:
  std::vector<HMODULE> dlls_;
  HWND hwnd_ = nullptr;
  void* device_ = nullptr;
  void* map_ = nullptr;
};

}  // namespace detail

Adapter* create_adapter() {
  return new detail::AdapterImpl();
}

}  // namespace gpu
