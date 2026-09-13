// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE. Browser chrome is app::BrowserView; this file only
// dispatches ContentMain (--type=gpu / --type=renderer) and --self-test.

#include <windows.h>
#include <shellapi.h>

#include "app/views/browser_view.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "gpu/gpu.h"
#include "ui/views/catalog_view.h"
#include "ui/views/map_viewport.h"
#include "ui/views/view.h"

namespace {

bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

void pump_briefly(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

int BrowserMain(const content::ContentMainParams&) {
  SetProcessDPIAware();
  const bool self_test = cmd_has_self_test();

  app::BrowserView browser;
  if (!browser.init()) {
    return 1;
  }
  browser.show();
  if (self_test) {
    pump_briefly(400);
    if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
      return 2;
    }
    ui::views::MapViewport* map = browser.map_viewport();
    if (map &&
        map->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !map->wait_ready(20000)) {
      return 3;
    }
    ui::views::View* root = browser.contents_view();
    if (!root || root->child_count() < 3) {
      return 4;
    }
    ui::views::View* columns = root->child_at(1);
    if (!columns || columns->child_count() < 2) {
      return 5;
    }
    ui::views::CatalogView* catalog = browser.catalog_view();
    if (!catalog || !catalog->layer_tree()) {
      return 6;
    }
    return 0;
  }
  return browser.run_loop();
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
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
