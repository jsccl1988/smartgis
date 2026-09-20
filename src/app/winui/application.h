// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_APPLICATION_H_
#define APP_WINUI_APPLICATION_H_

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <memory>

#include "app/winui/main_window.h"

namespace app {
namespace winui {
namespace detail {

// WinUI Application + XamlControls metadata so NavigationView templates load.
struct App : winrt::Microsoft::UI::Xaml::ApplicationT<
                 App,
                 winrt::Microsoft::UI::Xaml::Markup::IXamlMetadataProvider> {
  App();

  void OnLaunched(
      winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const& args);

  static bool is_self_test_cmd();
  static bool is_exit_teardown_test_cmd();
  static int self_test_exit_code();

  winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(
      winrt::Windows::UI::Xaml::Interop::TypeName const& type);
  winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(
      winrt::hstring const& full_name);
  winrt::com_array<winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition>
  GetXmlnsDefinitions();

 private:
  winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider
      provider_;
  std::unique_ptr<MainWindow> main_window_;
};

}  // namespace detail
}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_APPLICATION_H_
