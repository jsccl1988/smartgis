// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WEBVIEW2_MAP_PANE_H_
#define APP_WEBVIEW2_MAP_PANE_H_

#include <windows.h>

#include "content/public/map_contents.h"

namespace app {
namespace webview2 {

// Sibling native HWND for the map. Not a WebView / WebGL surface.
class MapPane {
 public:
  MapPane();
  ~MapPane();

  bool create(HWND parent, content::MapContents* session, uint32_t view_id);
  void move(int x, int y, int w, int h, float dpi);
  void invalidate();
  HWND hwnd() const { return hwnd_; }

  void set_oop(bool oop);
  void set_probe_text(const wchar_t* text);

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
  void paint();
  void present_or_fallback(HDC hdc, int w, int h);
  void dispatch_mouse(UINT msg, WPARAM wp, LPARAM lp);

  HWND hwnd_ = nullptr;
  content::MapContents* session_ = nullptr;
  uint32_t view_id_ = 0;
  bool oop_ = false;
  const wchar_t* probe_ = L"";
  content::Extent2 local_extent_{-180.0, -90.0, 180.0, 90.0};
  bool dragging_ = false;
  int last_x_ = 0;
  int last_y_ = 0;
};

}  // namespace webview2
}  // namespace app

#endif  // APP_WEBVIEW2_MAP_PANE_H_
