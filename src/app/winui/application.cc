// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/application.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace app {
namespace winui {
namespace detail {
namespace {

int g_self_test_exit = 0;

}  // namespace

App::App() {}

bool App::is_self_test_cmd() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

int App::self_test_exit_code() {
  return g_self_test_exit;
}

void App::OnLaunched(
    ::winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
  try {
    Resources().MergedDictionaries().Append(
        ::winrt::Microsoft::UI::Xaml::Controls::XamlControlsResources());
  } catch (::winrt::hresult_error const&) {
  }
  try {
    main_window_ = std::make_unique<MainWindow>();
    main_window_->activate();
    if (is_self_test_cmd()) {
      HWND hwnd = main_window_->native_hwnd();
      if (!hwnd || !IsWindow(hwnd)) {
        g_self_test_exit = 2;
      }
      ::winrt::Microsoft::UI::Xaml::Application::Current().Exit();
    }
  } catch (::winrt::hresult_error const&) {
    if (is_self_test_cmd()) {
      g_self_test_exit = 3;
      ::winrt::Microsoft::UI::Xaml::Application::Current().Exit();
    }
  }
}

::winrt::Microsoft::UI::Xaml::Markup::IXamlType App::GetXamlType(
    ::winrt::Windows::UI::Xaml::Interop::TypeName const& type) {
  return provider_.GetXamlType(type);
}

::winrt::Microsoft::UI::Xaml::Markup::IXamlType App::GetXamlType(
    ::winrt::hstring const& full_name) {
  return provider_.GetXamlType(full_name);
}

::winrt::com_array<::winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition>
App::GetXmlnsDefinitions() {
  return provider_.GetXmlnsDefinitions();
}

}  // namespace detail
}  // namespace winui
}  // namespace app
