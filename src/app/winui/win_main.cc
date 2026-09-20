// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/application.h"
#include "app/winui/detail/bootstrap.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "gpu/gpu.h"

#include <windows.h>
#include <shellapi.h>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

namespace {

int BrowserMain(const content::ContentMainParams&) {
  winrt::init_apartment(winrt::apartment_type::single_threaded);

  if (!app::winui::detail::initialize_windows_app_sdk()) {
    return 1;
  }

  winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    winrt::make<app::winui::detail::App>();
  });

  app::winui::detail::shutdown_windows_app_sdk();
  if (app::winui::detail::App::is_self_test_cmd() ||
      app::winui::detail::App::is_exit_teardown_test_cmd()) {
    return app::winui::detail::App::self_test_exit_code();
  }
  return 0;
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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
