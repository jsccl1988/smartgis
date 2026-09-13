// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_MAIN_WINDOW_H_
#define APP_WINUI_MAIN_WINDOW_H_

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <memory>

#include "detail/map_session.h"
#include "map_host.h"

namespace app {
namespace winui {

// Top-level Fluent chrome: NavigationView pane + map region. Dock comes later.
class MainWindow {
 public:
  MainWindow();
  ~MainWindow();

  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  void activate();
  HWND native_hwnd() const;

 private:
  void build_chrome();
  void attach_map();

  winrt::Microsoft::UI::Xaml::Window window_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::NavigationView nav_{nullptr};
  std::unique_ptr<MapHost> map_host_;
  content::MapContents* session_ = nullptr;
};

}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_MAIN_WINDOW_H_
