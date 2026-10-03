// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/widget_window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <imm.h>
#include <windowsx.h>

#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/shell/dialog_host.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/widget/widget.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

namespace ui {
namespace views {
namespace {

const wchar_t kWidgetClass[] = L"SmartGisViewsWidget";

}  // namespace

bool ensure_widget_window_class(WNDPROC wnd_proc) {
  static bool registered = false;
  if (registered) {
    return true;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  // NULL_BRUSH: never flash system COLOR_WINDOW behind Skia shell.
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
  wc.lpszClassName = kWidgetClass;
  registered = RegisterClassExW(&wc) != 0;
  return registered;
}

WidgetWindowCreate compute_widget_window_create(
    const Widget::InitParams& params) {
  WidgetWindowCreate out;
  const bool custom = params.frame_kind == Widget::FrameKind::kCustom;
  const bool popup = params.frame_kind == Widget::FrameKind::kPopup;
  // Custom frame must not include WS_CAPTION: DWM would still paint the OS
  // title bar even when WM_NCCALCSIZE collapses NC, causing a double caption
  // (white system bar + FrameView). Match owned dialogs: WS_POPUP + thickframe.
  // Top-level adds min/max boxes; WS_EX_APPWINDOW keeps a taskbar button.
  // kPopup is a thin-border dropdown owned by another HWND (no caption).
  if (popup) {
    out.style = WS_POPUP | WS_CLIPCHILDREN;
    out.ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  } else {
    out.style =
        custom
            ? (WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_CLIPCHILDREN |
               (params.owner ? 0u : (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)))
            : (params.owner ? kOwnedDialogStyle
                            : (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN));
    out.ex_style = (custom && !params.owner) ? WS_EX_APPWINDOW : 0u;
  }
  out.x = CW_USEDEFAULT;
  out.y = CW_USEDEFAULT;
  out.width = params.width;
  out.height = params.height;

  if (popup) {
    float scale = scale_factor_from_dpi(
        dpi_for_hwnd(params.owner ? params.owner : nullptr));
    int client_w =
        params.size_in_dips ? dip_to_px(params.width, scale) : params.width;
    int client_h =
        params.size_in_dips ? dip_to_px(params.height, scale) : params.height;
    if (client_w < 40) {
      client_w = 40;
    }
    if (client_h < 20) {
      client_h = 20;
    }
    // Borderless popup: CreateWindow size == client (WM_NCCALCSIZE collapses).
    out.width = client_w;
    out.height = client_h;
    if (params.has_screen_origin) {
      out.x = params.screen_x;
      out.y = params.screen_y;
    } else if (params.owner && IsWindow(params.owner)) {
      RECT owner_rc = {};
      GetWindowRect(params.owner, &owner_rc);
      out.x = owner_rc.left;
      out.y = owner_rc.bottom;
    }
    clamp_rect_to_work_area(&out.x, &out.y, out.width, out.height,
                            params.owner);
    return out;
  }

  if (params.owner) {
    if (custom) {
      float scale = scale_factor_from_dpi(dpi_for_hwnd(params.owner));
      int client_w = params.size_in_dips ? dip_to_px(params.width, scale)
                                         : params.width;
      int client_h = params.size_in_dips ? dip_to_px(params.height, scale)
                                         : params.height;
      // Custom frame: CreateWindow size == client (WM_NCCALCSIZE collapses NC).
      // Add caption height into the requested client so dialogs keep body size.
      client_h += dip_to_px(32, scale);
      RECT owner_rc = {};
      if (IsWindow(params.owner)) {
        GetWindowRect(params.owner, &owner_rc);
      }
      OwnedPopupGeom place =
          center_outer_on_owner_rect(owner_rc, client_w, client_h);
      out.x = place.x;
      out.y = place.y;
      out.width = place.outer_width;
      out.height = place.outer_height;
    } else {
      // Dialog callers pass client size (DIPs when size_in_dips). Shared host
      // scales, expands via AdjustWindowRectEx, centers on owner, clamps work.
      OwnedPopupGeom place;
      if (params.size_in_dips) {
        place = place_owned_dialog(params.owner, params.width, params.height,
                                   out.style, 0);
      } else {
        const float scale =
            scale_factor_from_dpi(dpi_for_hwnd(params.owner));
        place =
            place_owned_dialog(params.owner, px_to_dip(params.width, scale),
                               px_to_dip(params.height, scale), out.style, 0);
      }
      out.x = place.x;
      out.y = place.y;
      out.width = place.outer_width;
      out.height = place.outer_height;
    }
    return out;
  }

  // Top-level shell: InitParams is *client* size. Scale DIPs, expand to
  // outer CreateWindow box (system frame) or keep as-is (custom CSD).
  int client_w = params.width;
  int client_h = params.height;
  if (params.size_in_dips) {
    const float scale = scale_factor_from_dpi(dpi_for_hwnd(nullptr));
    client_w = dip_to_px(params.width, scale);
    client_h = dip_to_px(params.height, scale);
  }
  if (client_w < 160) {
    client_w = 160;
  }
  if (client_h < 120) {
    client_h = 120;
  }
  if (custom) {
    out.width = client_w;
    out.height = client_h;
  } else {
    client_to_outer_size(client_w, client_h, out.style, 0, &out.width,
                         &out.height);
  }
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  POINT origin = {};
  HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
  if (mon && GetMonitorInfoW(mon, &mi)) {
    const int work_w = mi.rcWork.right - mi.rcWork.left;
    const int work_h = mi.rcWork.bottom - mi.rcWork.top;
    if (work_w > 0 && out.width > work_w) {
      out.width = work_w;
    }
    if (work_h > 0 && out.height > work_h) {
      out.height = work_h;
    }
  }
  return out;
}

HWND create_widget_hwnd(const WidgetWindowCreate& create,
                        const Widget::InitParams& params,
                        void* create_param) {
  return CreateWindowExW(create.ex_style, kWidgetClass, params.title,
                         create.style, create.x, create.y, create.width,
                         create.height, params.owner, nullptr,
                         GetModuleHandleW(nullptr), create_param);
}

void raise_owned_widget_above_owner(HWND hwnd, HWND owner) {
  if (!hwnd || !owner || !IsWindow(owner)) {
    return;
  }
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

LRESULT hit_test_custom_frame(HWND hwnd,
                              int screen_x,
                              int screen_y,
                              float device_scale_factor,
                              View* contents) {
  if (!hwnd) {
    return HTCLIENT;
  }
  POINT pt = {screen_x, screen_y};
  ScreenToClient(hwnd, &pt);
  RECT cr = {};
  GetClientRect(hwnd, &cr);
  const int w = cr.right - cr.left;
  const int h = cr.bottom - cr.top;
  const int border = (std::max)(1, dip_to_px(6, device_scale_factor));

  WINDOWPLACEMENT wp = {};
  wp.length = sizeof(wp);
  GetWindowPlacement(hwnd, &wp);
  const bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

  if (!maximized) {
    const bool left = pt.x < border;
    const bool right = pt.x >= w - border;
    const bool top = pt.y < border;
    const bool bottom = pt.y >= h - border;
    if (top && left) {
      return HTTOPLEFT;
    }
    if (top && right) {
      return HTTOPRIGHT;
    }
    if (bottom && left) {
      return HTBOTTOMLEFT;
    }
    if (bottom && right) {
      return HTBOTTOMRIGHT;
    }
    if (left) {
      return HTLEFT;
    }
    if (right) {
      return HTRIGHT;
    }
    if (top) {
      return HTTOP;
    }
    if (bottom) {
      return HTBOTTOM;
    }
  }

  auto* frame = dynamic_cast<FrameView*>(contents);
  const int caption_h = frame ? frame->caption_height_px()
                              : dip_to_px(32, device_scale_factor);
  if (pt.y >= 0 && pt.y < caption_h) {
    if (frame && frame->point_in_caption_controls(pt.x, pt.y)) {
      return HTCLIENT;
    }
    return HTCAPTION;
  }
  return HTCLIENT;
}

Widget* Widget::from_hwnd(HWND hwnd) {
  return reinterpret_cast<Widget*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK Widget::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Widget*>(cs->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  Widget* self = from_hwnd(hwnd);
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return self->handle_message(hwnd, msg, wparam, lparam);
}

LRESULT Widget::handle_nc_hit_test(int screen_x, int screen_y) {
  return hit_test_custom_frame(hwnd_, screen_x, screen_y, device_scale_factor_,
                               contents_.get());
}

LRESULT Widget::handle_message(HWND hwnd, UINT msg, WPARAM wparam,
                               LPARAM lparam) {
  switch (msg) {
    case WM_NCCALCSIZE:
      if (frame_kind_ == FrameKind::kCustom ||
          frame_kind_ == FrameKind::kPopup) {
        // Client area fills the entire window; FrameView paints the caption.
        // Handle both wParam TRUE and FALSE — falling through to DefWindowProc
        // would re-apply WS_CAPTION insets and show a second OS title bar.
        // kPopup also collapses NC so WS_BORDER is drawn by DWM as a thin edge
        // while client size matches InitParams.
        return 0;
      }
      break;
    case WM_NCHITTEST:
      if (frame_kind_ == FrameKind::kCustom) {
        return handle_nc_hit_test(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
      }
      break;
    case WM_NCLBUTTONDBLCLK:
      if (frame_kind_ == FrameKind::kCustom && wparam == HTCAPTION) {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);
        ShowWindow(hwnd, (wp.showCmd == SW_SHOWMAXIMIZED) ? SW_RESTORE
                                                          : SW_MAXIMIZE);
        if (auto* frame = dynamic_cast<FrameView*>(contents_.get())) {
          frame->sync_maximize_button(wp.showCmd != SW_SHOWMAXIMIZED);
        }
        return 0;
      }
      break;
    case WM_SIZE:
      if (frame_kind_ == FrameKind::kCustom) {
        if (auto* frame = dynamic_cast<FrameView*>(contents_.get())) {
          frame->sync_maximize_button(wparam == SIZE_MAXIMIZED);
        }
      }
      on_size(LOWORD(lparam), HIWORD(lparam));
      return 0;
    case WM_PAINT:
      on_paint();
      return 0;
    case kShellPublishedMessage:
      on_shell_published_message(static_cast<std::uint64_t>(wparam));
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_GETDPISCALEDSIZE: {
      auto* size = reinterpret_cast<SIZE*>(lparam);
      const unsigned new_dpi = static_cast<unsigned>(wparam);
      if (!size || dpi_ == 0 || new_dpi == 0) {
        break;
      }
      RECT wr = {};
      GetWindowRect(hwnd, &wr);
      const float ratio =
          static_cast<float>(new_dpi) / static_cast<float>(dpi_);
      size->cx = static_cast<LONG>(
          std::lround((wr.right - wr.left) * static_cast<double>(ratio)));
      size->cy = static_cast<LONG>(
          std::lround((wr.bottom - wr.top) * static_cast<double>(ratio)));
      return TRUE;
    }
    case WM_DPICHANGED: {
      const unsigned new_dpi = static_cast<unsigned>(HIWORD(wparam));
      const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
      on_dpi_changed(new_dpi, suggested);
      return 0;
    }
    case WM_LBUTTONDOWN:
      SetCapture(hwnd_);
      dispatch_mouse(MouseEvent::Type::kDown, wparam, lparam, 1, 0);
      return 0;
    case WM_LBUTTONUP:
      if (GetCapture() == hwnd_) {
        ReleaseCapture();
      }
      dispatch_mouse(MouseEvent::Type::kUp, wparam, lparam, 1, 0);
      return 0;
    case WM_LBUTTONDBLCLK:
      dispatch_mouse(MouseEvent::Type::kDblClick, wparam, lparam, 1, 0);
      return 0;
    case WM_RBUTTONDOWN:
      dispatch_mouse(MouseEvent::Type::kDown, wparam, lparam, 2, 0);
      return 0;
    case WM_RBUTTONUP:
      dispatch_mouse(MouseEvent::Type::kUp, wparam, lparam, 2, 0);
      return 0;
    case WM_MOUSEMOVE:
      track_mouse_leave();
      dispatch_mouse(MouseEvent::Type::kMove, wparam, lparam, 0, 0);
      return 0;
    case WM_MOUSELEAVE:
      tracking_leave_ = false;
      update_hover(nullptr);
      return 0;
    case WM_MOUSEWHEEL:
      dispatch_mouse(MouseEvent::Type::kWheel, wparam, lparam, 0,
                     GET_WHEEL_DELTA_WPARAM(wparam));
      return 0;
    case WM_KEYDOWN:
      if (modal_ && wparam == VK_ESCAPE) {
        request_close();
        return 0;
      }
      dispatch_key(KeyEvent::Type::kDown, wparam, lparam);
      return 0;
    case WM_KEYUP:
      dispatch_key(KeyEvent::Type::kUp, wparam, lparam);
      return 0;
    case WM_CHAR: {
      CharEvent e;
      e.ch = static_cast<wchar_t>(wparam);
      send_char(e);
      return 0;
    }
    case WM_IME_COMPOSITION: {
      if (!focused_) {
        break;
      }
      HIMC imc = ImmGetContext(hwnd);
      if (!imc) {
        break;
      }
      bool handled = false;
      if (lparam & GCS_RESULTSTR) {
        const LONG bytes =
            ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
        if (bytes > 0) {
          std::wstring result(static_cast<size_t>(bytes / sizeof(wchar_t)),
                              L'\0');
          ImmGetCompositionStringW(imc, GCS_RESULTSTR, result.data(),
                                   static_cast<DWORD>(bytes));
          handled = focused_->on_ime_composition(result, true);
        } else {
          handled = focused_->on_ime_composition(L"", true);
        }
      } else if (lparam & GCS_COMPSTR) {
        const LONG bytes =
            ImmGetCompositionStringW(imc, GCS_COMPSTR, nullptr, 0);
        if (bytes > 0) {
          std::wstring comp(static_cast<size_t>(bytes / sizeof(wchar_t)),
                            L'\0');
          ImmGetCompositionStringW(imc, GCS_COMPSTR, comp.data(),
                                   static_cast<DWORD>(bytes));
          handled = focused_->on_ime_composition(comp, false);
        } else {
          handled = focused_->on_ime_composition(L"", false);
        }
      }
      ImmReleaseContext(hwnd, imc);
      if (handled) {
        return 0;
      }
      break;
    }
    case WM_IME_ENDCOMPOSITION:
      if (focused_) {
        focused_->on_ime_composition(L"", false);
      }
      break;
    case WM_TIMER:
      // Caret blink: Textfield arms this id while focused.
      if (wparam == 0x43415245u /* 'CARE' */ && focused_) {
        focused_->schedule_paint();
        return 0;
      }
      break;
    case WM_ACTIVATE:
      if (dismiss_on_deactivate_ &&
          LOWORD(wparam) == WA_INACTIVE) {
        request_close();
        return 0;
      }
      break;
    case WM_CLOSE:
      fire_will_close();
      if (modal_) {
        request_close();
        return 0;
      }
      break;
    case WM_DESTROY:
      // User close (Alt+F4 / X) ends run_loop. ~Widget DestroyWindow must
      // not PostQuitMessage: that poisons a console test thread.
      // Modal dialogs and kPopup dropdowns must also skip it so the owner
      // message loop stays alive.
      if (!destroying_ && !modal_ && frame_kind_ != FrameKind::kPopup) {
        PostQuitMessage(0);
      }
      return 0;
    case WM_NCDESTROY:
      hwnd_ = nullptr;
      return DefWindowProcW(hwnd, msg, wparam, lparam);
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace views
}  // namespace ui
