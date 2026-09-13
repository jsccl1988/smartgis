// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WEBVIEW2_HOST_WINDOW_H_
#define APP_WEBVIEW2_HOST_WINDOW_H_

#include <windows.h>

#include "content/public/map_contents.h"
#include "map_pane.h"

namespace app {
namespace webview2 {

class WebViewChrome;

// Top-level Win32 host: WebView2 chrome + sibling map HWND.
class HostWindow : public content::MapContentsObserver {
 public:
  HostWindow();
  ~HostWindow() override;

  bool create(HINSTANCE instance, const wchar_t* webview_fixed);
  int run_loop();

  void on_map_slot(int x, int y, int w, int h);
  void on_activate_tool(const char* tool_id);
  void on_catalog_op(const char* json);

  void OnFrameReady(uint32_t view_id, uint32_t generation) override;
  void OnRenderDied() override;

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
  void on_size();
  void layout_fallback_map();

  HINSTANCE instance_ = nullptr;
  HWND hwnd_ = nullptr;
  content::MapContents* session_ = nullptr;
  uint32_t view_id_ = 0;
  MapPane pane_;
  WebViewChrome* chrome_ = nullptr;
  wchar_t webview_fixed_[MAX_PATH] = {};
};

}  // namespace webview2
}  // namespace app

#endif  // APP_WEBVIEW2_HOST_WINDOW_H_
