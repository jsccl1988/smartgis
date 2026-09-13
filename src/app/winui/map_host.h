// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_MAP_HOST_H_
#define APP_WINUI_MAP_HOST_H_

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "app/winui/detail/map_session.h"

namespace app {
namespace winui {

// Presents the map inside the WinUI tree. SwapChainPanel when the session
// exposes a DXGI shared handle; otherwise a child HWND over the map slot.
class MapHost {
 public:
  MapHost();
  ~MapHost();

  MapHost(const MapHost&) = delete;
  MapHost& operator=(const MapHost&) = delete;

  winrt::Microsoft::UI::Xaml::UIElement root_element() const;

  void attach_session(content::MapContents* session, HWND window_hwnd);
  void sync_layout();
  const wchar_t* present_path() const;
  const wchar_t* process_path() const;

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam);

 private:
  bool try_attach_swap_chain();
  void attach_child_hwnd();
  void destroy_child_hwnd();
  void paint_child() const;

  winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};

  content::MapContents* session_ = nullptr;
  content::MapWidgetHostView* view_ = nullptr;
  HWND window_hwnd_ = nullptr;
  HWND child_hwnd_ = nullptr;
  uint32_t view_id_ = 0;
  bool swap_chain_ = false;
};

}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_MAP_HOST_H_
