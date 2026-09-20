// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_MAIN_WINDOW_H_
#define APP_WINUI_MAIN_WINDOW_H_

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <memory>
#include <string_view>
#include <thread>
#include <atomic>

#include "app/winui/detail/map_session.h"
#include "app/winui/map_host.h"

namespace app {
namespace winui {

// Product chrome aligned with Views IDE: MenuBar, Catalog, Map|Data|3D,
// Ambox, Inspector, StatusBar. Toolkit is WinUI; regions and command ids match.
class MainWindow {
 public:
  MainWindow();
  ~MainWindow();

  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  void activate();
  HWND native_hwnd() const;
  MapHost* map_host() const { return map_host_.get(); }

  // Ordered teardown for File→Exit / Window.Closed / Application::Exit.
  // Idempotent: safe to call from Closed and from the destructor.
  void shutdown();

  // True when Menu / Catalog / Ambox / Inspector / Status / map tabs exist.
  bool has_ide_chrome() const;
  // Switch Map Edit (0) / Data (1) / 3D (2). Used by --self-test.
  void select_map_tab(int index);
  int active_map_tab() const { return active_tab_; }
  // Activate a Workspace-style tool id via MapContents::ActivateTool.
  bool run_tool_command(std::string_view command_id);
  void set_status(std::wstring_view text);

 private:
  void build_chrome();
  void attach_map();
  void wire_menu();
  void wire_catalog();
  void wire_ambox();
  void wire_inspector();
  void wire_map_tabs();
  void on_open();
  void on_exit();
  void highlight_map_tab(int index);
  void wire_closed();

  winrt::Microsoft::UI::Xaml::Window window_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::MenuBar menu_bar_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Grid catalog_panel_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TreeView catalog_tree_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::StackPanel ambox_panel_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Grid map_column_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button tab_map_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button tab_data_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button tab_scene_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Grid inspector_panel_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button insp_feature_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button insp_attrs_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock inspector_body_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock status_bar_{nullptr};

  std::unique_ptr<MapHost> map_host_;
  content::MapContents* session_ = nullptr;
  std::thread render_thread_;
  winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer size_heal_timer_{
      nullptr};
  winrt::event_token closed_token_{};
  bool shutting_down_ = false;
  int active_tab_ = 0;
};

}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_MAIN_WINDOW_H_
