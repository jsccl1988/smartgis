// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Product chrome for scheme 3: Views + Skia + hosted map HWND.
// Same PE relaunches as --type=gpu / --type=renderer (ContentMain).

#include <memory>

#include <windows.h>
#include <shellapi.h>

#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "gpu/gpu.h"
#include "render/skia/canvas.h"
#include "ui/views/layout.h"
#include "ui/views/map_viewport.h"
#include "ui/views/view.h"
#include "ui/views/widget.h"

namespace {

class CatalogView : public ui::views::View {
 public:
  CatalogView() { set_preferred_size({240, 0}); }

 protected:
  void paint_self(render::skia::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const ui::views::Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height,
                      render::skia::color_rgb(37, 37, 38));
    canvas->fill_rect(b.x, b.y, b.width, 36,
                      render::skia::color_rgb(0, 122, 204));
    canvas->draw_text(b.x + 12, b.y + 10, L"Catalog",
                      render::skia::color_rgb(255, 255, 255));
    canvas->draw_text(b.x + 12, b.y + 52, L"Maps",
                      render::skia::color_rgb(200, 200, 200));
    canvas->draw_text(b.x + 24, b.y + 76, L"(placeholder)",
                      render::skia::color_rgb(140, 140, 140));
    canvas->draw_text(b.x + 12, b.y + 108, L"Data sources",
                      render::skia::color_rgb(200, 200, 200));
    canvas->draw_text(b.x + 24, b.y + 132, L"(placeholder)",
                      render::skia::color_rgb(140, 140, 140));
  }
};

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

  ui::views::Widget widget;
  ui::views::Widget::InitParams params;
  params.title = L"SmartGIS Views";
  params.width = 1280;
  params.height = 800;
  if (!widget.init(params)) {
    return 1;
  }

  auto root = std::make_unique<ui::views::View>();
  auto box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kHorizontal);
  auto catalog = std::make_unique<CatalogView>();
  auto map = std::make_unique<ui::views::MapViewport>();
  ui::views::MapViewport* map_ptr = map.get();
  box->set_flex_for_view(map_ptr, 1);
  root->set_layout_manager(std::move(box));
  root->add_child(std::move(catalog));
  root->add_child(std::move(map));

  widget.set_contents_view(std::move(root));
  map_ptr->attach();
  widget.show();
  if (self_test) {
    pump_briefly(400);
    if (!widget.hwnd() || !IsWindow(widget.hwnd())) {
      return 2;
    }
    if (map_ptr->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !map_ptr->wait_ready(20000)) {
      return 3;
    }
    return 0;
  }
  return widget.run_loop();
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
