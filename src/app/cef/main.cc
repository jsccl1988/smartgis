// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CEF product chrome PE. Order: CefExecuteProcess → ContentMain
// (--type=gpu / --type=renderer) → BrowserMain (layout + CEF + map slots).

#include <windows.h>
#include <shellapi.h>

#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"

#include "app/cef/cef_app.h"
#include "app/cef/cef_browser_host.h"
#include "app/cef/cef_map_slot.h"
#include "app/cef/chrome_bridge.h"
#include "app/cef/layout_host.h"
#include "app/cef/self_test.h"
#include "app/views/map_scene.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "content/public/map_contents.h"
#include "gpu/gpu.h"

#include <memory>
#include <string>

namespace {

bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

struct BrowserState {
  app::cef::LayoutHost layout;
  CefRefPtr<app::cef::CefBrowserHost> browser_host;
  app::cef::ChromeBridge bridge;
  app::MapScene document;
  content::MapContents* session = nullptr;
  app::cef::CefMapSlot slots[3];
  bool self_test = false;
};

void on_layout_resize(void* user) {
  auto* st = static_cast<BrowserState*>(user);
  if (!st) {
    return;
  }
  if (st->browser_host) {
    st->browser_host->resize(st->layout.chrome_rect());
  }
  const int active = st->layout.active_tab();
  for (int i = 0; i < 3; ++i) {
    st->slots[i].set_visible(i == active);
    if (i == active) {
      st->slots[i].sync_layout(st->layout.map_slot_rect(),
                               st->layout.dpi_scale());
    }
  }
}

int BrowserMain(const content::ContentMainParams& params) {
  auto state = std::make_unique<BrowserState>();
  state->self_test = cmd_has_self_test();

  if (!state->layout.create(params.instance)) {
    return 1;
  }
  state->layout.set_resize_callback(&on_layout_resize, state.get());

  CefMainArgs main_args(static_cast<HINSTANCE>(params.instance));
  CefRefPtr<app::cef::CefApp> app(new app::cef::CefApp());
  CefSettings settings;
  settings.no_sandbox = true;
  settings.windowless_rendering_enabled = false;
  // Keep CEF profile under the exe directory (writable, not LocalAppData).
  {
    wchar_t root[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, root, MAX_PATH);
    for (int i = static_cast<int>(wcslen(root)) - 1; i >= 0; --i) {
      if (root[i] == L'\\' || root[i] == L'/') {
        root[i] = L'\0';
        break;
      }
    }
    const std::wstring cache = std::wstring(root) + L"\\cef_user_data";
    CreateDirectoryW(cache.c_str(), nullptr);
    CefString(&settings.cache_path).FromWString(cache);
    const std::wstring log = std::wstring(root) + L"\\cef_debug.log";
    CefString(&settings.log_file).FromWString(log);
    settings.log_severity = LOGSEVERITY_INFO;
  }
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return 40;
  }

  state->session = content::MapContents::Create();
  const bool render_ok =
      state->session && state->session->StartRenderProcess();

  state->slots[0].create(state->layout.hwnd(), state->session,
                         content::ViewKind::kMapEdit);
  state->slots[1].create(state->layout.hwnd(), state->session,
                         content::ViewKind::kMapData);
  state->slots[2].create(state->layout.hwnd(), state->session,
                         content::ViewKind::kScene3d);
  for (int i = 0; i < 3; ++i) {
    state->slots[i].set_render_ok(render_ok);
  }

  state->bridge.set_handlers(&state->layout, state->slots, 3, state->session);
  state->bridge.set_document(&state->document);
  state->bridge.set_message_box_suppressed(state->self_test);
  state->bridge.seed_map_document();

  state->browser_host = new app::cef::CefBrowserHost();
  state->browser_host->set_bridge(&state->bridge);
  const std::wstring url = state->layout.cef_web_index_url();
  if (!state->browser_host->create(state->layout.hwnd(),
                                   state->layout.chrome_rect(), url)) {
    CefShutdown();
    return 40;
  }

  state->layout.set_active_tab(0);
  on_layout_resize(state.get());
  state->layout.show();

  // Wait for main frame + push Ready once both web and a map slot exist.
  const bool web_ok = state->browser_host->wait_load(20000);
  if (web_ok && state->slots[0].native_hwnd()) {
    state->bridge.notify_ready();
  }

  if (state->self_test) {
    const int rc =
        app::cef::run_self_test(state->layout, state->bridge, state->slots,
                                web_ok);
    for (int i = 0; i < 3; ++i) {
      state->slots[i].destroy();
    }
    if (state->session) {
      state->session->Shutdown();
    }
    state->browser_host->destroy();
    CefShutdown();
    return rc;
  }

  CefRunMessageLoop();

  for (int i = 0; i < 3; ++i) {
    state->slots[i].destroy();
  }
  if (state->session) {
    state->session->Shutdown();
  }
  state->browser_host->destroy();
  CefShutdown();
  return 0;
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  // If this is a MapContents OOP child (--pipe=), skip CefExecuteProcess so
  // CEF cannot swallow this repo's --type=gpu|renderer.
  const wchar_t* cmd = GetCommandLineW();
  const bool smt_oop = cmd && wcsstr(cmd, L"--pipe=");
  CefMainArgs main_args(instance);
  CefRefPtr<app::cef::CefApp> app(new app::cef::CefApp());
  if (!smt_oop) {
    const int cef_rc = CefExecuteProcess(main_args, app.get(), nullptr);
    if (cef_rc >= 0) {
      return cef_rc;
    }
  }

  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.browser_main = &BrowserMain;
  params.gpu_main = &GpuMain;
  params.renderer_main = &content::RendererMain;
  const int rc = content::ContentMain(params);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
