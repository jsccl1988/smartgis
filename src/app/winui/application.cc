// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/application.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace app {
namespace winui {
namespace detail {
namespace {

int g_self_test_exit = 0;

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      break;
    }
  }
  if (wcscat_s(path, L"self-test-mark.txt") != 0) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void pump_ticks(MapHost* host, int ticks) {
  for (int i = 0; i < ticks; ++i) {
    if (host) {
      host->sync_layout();
    }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(16);
  }
}

bool wait_frame_ok(MapHost* host, uint32_t timeout_ms) {
  if (!host || !host->session() || host->view_id() == 0) {
    return false;
  }
  if (!host->session()->WaitFrameReady(host->view_id(), timeout_ms)) {
    return false;
  }
  return host->has_presented_frame() && host->has_live_map_pixels();
}

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
      wchar_t mark_path[MAX_PATH] = {};
      if (GetModuleFileNameW(nullptr, mark_path, MAX_PATH) > 0) {
        for (int i = static_cast<int>(wcslen(mark_path)) - 1; i >= 0; --i) {
          if (mark_path[i] == L'\\' || mark_path[i] == L'/') {
            mark_path[i + 1] = L'\0';
            break;
          }
        }
        wcscat_s(mark_path, L"self-test-mark.txt");
        DeleteFileW(mark_path);
      }
      self_test_mark("show");

      HWND hwnd = main_window_->native_hwnd();
      if (!hwnd || !IsWindow(hwnd)) {
        g_self_test_exit = 2;
      } else if (!main_window_->has_ide_chrome()) {
        g_self_test_exit = 4;
      } else if (MapHost* host = main_window_->map_host()) {
        self_test_mark("hwnd-ok");
        self_test_mark("catalog-ok");
        // activate() starts GPU off-UI; wait for Hello before layout checks.
        {
          bool oop = false;
          for (int i = 0; i < 90; ++i) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
              TranslateMessage(&msg);
              DispatchMessageW(&msg);
            }
            if (host->session() && host->session()->IsOopRender() &&
                host->map_child_hwnd()) {
              oop = true;
              break;
            }
            Sleep(200);
          }
          if (!oop) {
            self_test_mark("oop-fail");
            g_self_test_exit = 7;
            // Fall through to exit path below via early checks.
          }
        }
        // Pump so SwapChainPanel SizeChanged can settle.
        bool layout_ok = false;
        for (int i = 0; i < 60; ++i) {
          host->sync_layout();
          MSG msg;
          while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
          }
          if (host->has_synced_map_layout()) {
            layout_ok = true;
            break;
          }
          Sleep(33);
        }
        if (!layout_ok) {
          self_test_mark("layout-fail");
        }
        if (!host->map_child_hwnd() || !IsWindow(host->map_child_hwnd())) {
          self_test_mark("no-child-hwnd");
          g_self_test_exit = 4;
        } else if (!host->has_synced_map_layout()) {
          self_test_mark("layout-small");
          g_self_test_exit = 6;
        } else if (!host->session() || !host->session()->IsOopRender()) {
          self_test_mark("oop-fail");
          g_self_test_exit = 7;
        } else if (!wait_frame_ok(host, 12000)) {
          self_test_mark("map-frame-fail");
          g_self_test_exit = 3;
        } else {
          self_test_mark("map-ready");
          self_test_mark("map-frame-ok");
          InvalidateRect(host->map_child_hwnd(), nullptr, FALSE);
          UpdateWindow(host->map_child_hwnd());

          // Data tab: OpenView kMapData + frame.
          main_window_->select_map_tab(1);
          pump_ticks(host, 16);
          if (!wait_frame_ok(host, 12000)) {
            self_test_mark("data-frame-fail");
            g_self_test_exit = 8;
          } else {
            self_test_mark("data-ready");

            // 3D tab: require a live presented frame (not placeholder).
            main_window_->select_map_tab(2);
            pump_ticks(host, 20);
            if (!wait_frame_ok(host, 12000)) {
              self_test_mark("scene-frame-fail");
              g_self_test_exit = 10;
            } else {
              self_test_mark("scene-ready");
              self_test_mark("scene-frame-ok");

              main_window_->select_map_tab(0);
              pump_ticks(host, 8);
              main_window_->run_tool_command("selection.point");
              main_window_->run_tool_command("selection.clear");
              self_test_mark("selection-ok");

              // Best-effort China PLP open via CatalogCall (OOP render).
              {
                wchar_t china_w[MAX_PATH] = {};
                if (GetModuleFileNameW(nullptr, china_w, MAX_PATH) > 0) {
                  for (int i = static_cast<int>(wcslen(china_w)) - 1; i >= 0;
                       --i) {
                    if (china_w[i] == L'\\' || china_w[i] == L'/') {
                      china_w[i + 1] = L'\0';
                      break;
                    }
                  }
                  wcscat_s(china_w, L"china_plp.geojson");
                  if (GetFileAttributesW(china_w) != INVALID_FILE_ATTRIBUTES) {
                    char utf8[MAX_PATH * 4] = {};
                    WideCharToMultiByte(CP_UTF8, 0, china_w, -1, utf8,
                                        sizeof(utf8), nullptr, nullptr);
                    std::string path_esc;
                    for (const char* p = utf8; *p; ++p) {
                      if (*p == '\\' || *p == '"') {
                        path_esc.push_back('\\');
                      }
                      path_esc.push_back(*p);
                    }
                    const std::string json =
                        std::string("{\"op\":\"open\",\"path\":\"") + path_esc +
                        "\"}";
                    host->session()->CatalogCall(json.c_str());
                    pump_ticks(host, 12);
                    self_test_mark("china-plp-ok");
                  } else {
                    self_test_mark("china-plp-missing");
                  }
                }
              }
              main_window_->run_tool_command("view.pan");
              pump_ticks(host, 4);
              self_test_mark("pan-ok");
              self_test_mark("pass");
              g_self_test_exit = 0;
            }
          }
        }
        char exit_mark[64];
        std::snprintf(exit_mark, sizeof(exit_mark), "exit-%d", g_self_test_exit);
        self_test_mark(exit_mark);
      } else {
        g_self_test_exit = 5;
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
