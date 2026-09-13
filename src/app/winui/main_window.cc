// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "main_window.h"

#include <microsoft.ui.xaml.window.h>

#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>

namespace app {
namespace winui {
namespace {

::winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem make_item(
    ::winrt::hstring const& tag,
    ::winrt::hstring const& content) {
  ::winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem item;
  item.Tag(::winrt::box_value(tag));
  item.Content(::winrt::box_value(content));
  return item;
}

}  // namespace

MainWindow::MainWindow() {
  window_ = ::winrt::Microsoft::UI::Xaml::Window();
  window_.Title(L"SmartGIS");
  try {
    map_host_ = std::make_unique<MapHost>();
    session_ = content::MapContents::Create();
    if (session_) {
      session_->StartRenderProcess();
    }
    build_chrome();
  } catch (::winrt::hresult_error const&) {
    ::winrt::Microsoft::UI::Xaml::Controls::TextBlock hint;
    hint.Text(L"SmartGIS WinUI (Windows App Runtime 1.7). "
              L"NavigationView chrome failed; map host skipped.");
    hint.Margin(::winrt::Microsoft::UI::Xaml::Thickness{24, 24, 24, 24});
    hint.TextWrapping(::winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
    window_.Content(hint);
  }
}

MainWindow::~MainWindow() {
  if (session_) {
    session_->Shutdown();
    session_ = nullptr;
  }
}

void MainWindow::activate() {
  window_.Activate();
  attach_map();
}

HWND MainWindow::native_hwnd() const {
  HWND hwnd = nullptr;
  if (window_) {
    ::winrt::com_ptr<IWindowNative> native;
    auto* unk = static_cast<::IUnknown*>(::winrt::get_abi(window_));
    if (unk) {
      ::winrt::check_hresult(
          unk->QueryInterface(__uuidof(IWindowNative), native.put_void()));
    }
    if (native) {
      native->get_WindowHandle(&hwnd);
    }
  }
  return hwnd;
}

void MainWindow::build_chrome() {
  nav_ = ::winrt::Microsoft::UI::Xaml::Controls::NavigationView();
  nav_.PaneDisplayMode(
      ::winrt::Microsoft::UI::Xaml::Controls::NavigationViewPaneDisplayMode::Left);
  nav_.IsBackButtonVisible(
      ::winrt::Microsoft::UI::Xaml::Controls::NavigationViewBackButtonVisible::
          Collapsed);
  nav_.IsSettingsVisible(false);
  nav_.Header(::winrt::box_value(L"Map"));

  nav_.MenuItems().Append(make_item(L"map", L"Map"));
  nav_.MenuItems().Append(make_item(L"catalog", L"Catalog"));
  nav_.MenuItems().Append(make_item(L"tools", L"Tools"));

  auto map_item =
      nav_.MenuItems().GetAt(0).as<
          ::winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem>();
  nav_.SelectedItem(map_item);
  nav_.Content(map_host_->root_element());

  nav_.SelectionChanged(
      [this](::winrt::Microsoft::UI::Xaml::Controls::NavigationView const&,
             ::winrt::Microsoft::UI::Xaml::Controls::
                 NavigationViewSelectionChangedEventArgs const& args) {
        auto item = args.SelectedItem().try_as<
            ::winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (!item) {
          return;
        }
        const auto tag = ::winrt::unbox_value_or<::winrt::hstring>(item.Tag(), L"");
        if (tag == L"map") {
          nav_.Header(::winrt::box_value(L"Map"));
          nav_.Content(map_host_->root_element());
          map_host_->sync_layout();
        } else if (tag == L"catalog") {
          nav_.Header(::winrt::box_value(L"Catalog"));
          ::winrt::Microsoft::UI::Xaml::Controls::TextBlock hint;
          hint.Margin(::winrt::Microsoft::UI::Xaml::Thickness{16, 16, 16, 16});
          hint.Text(
              L"Catalog dock is not in v1. Layer / DS ops go through "
              L"content::MapSession::catalog_call once src/content lands.");
          hint.TextWrapping(::winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
          nav_.Content(hint);
        } else if (tag == L"tools") {
          nav_.Header(::winrt::box_value(L"Tools"));
          ::winrt::Microsoft::UI::Xaml::Controls::TextBlock hint;
          hint.Margin(::winrt::Microsoft::UI::Xaml::Thickness{16, 16, 16, 16});
          hint.Text(
              L"Toolbox dock is not in v1. ActivateTool is forwarded on the "
              L"map host pointer path.");
          hint.TextWrapping(::winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
          nav_.Content(hint);
        }
      });

  window_.Content(nav_);
}

void MainWindow::attach_map() {
  map_host_->attach_session(session_, native_hwnd());
}

}  // namespace winui
}  // namespace app
