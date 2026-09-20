// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/main_window.h"

#include <microsoft.ui.xaml.window.h>

#include <cstring>
#include <string>
#include <thread>
#include <atomic>

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>

namespace app {
namespace winui {
namespace {

using ::winrt::Microsoft::UI::Xaml::Controls::Button;
using ::winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition;
using ::winrt::Microsoft::UI::Xaml::Controls::Grid;
using ::winrt::Microsoft::UI::Xaml::Controls::MenuBar;
using ::winrt::Microsoft::UI::Xaml::Controls::MenuBarItem;
using ::winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem;
using ::winrt::Microsoft::UI::Xaml::Controls::RowDefinition;
using ::winrt::Microsoft::UI::Xaml::Controls::StackPanel;
using ::winrt::Microsoft::UI::Xaml::Controls::TextBlock;
using ::winrt::Microsoft::UI::Xaml::Controls::TreeView;
using ::winrt::Microsoft::UI::Xaml::Controls::TreeViewNode;
using ::winrt::Microsoft::UI::Xaml::GridLength;
using ::winrt::Microsoft::UI::Xaml::GridUnitType;
using ::winrt::Microsoft::UI::Xaml::HorizontalAlignment;
using ::winrt::Microsoft::UI::Xaml::Media::SolidColorBrush;
using ::winrt::Microsoft::UI::Xaml::Thickness;
using ::winrt::Microsoft::UI::Xaml::VerticalAlignment;
using ::winrt::Windows::UI::Colors;

std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

Button make_tab_button(::winrt::hstring const& label) {
  Button btn;
  btn.Content(::winrt::box_value(label));
  btn.Margin(Thickness{2, 2, 2, 2});
  btn.Padding(Thickness{10, 4, 10, 4});
  return btn;
}

void set_tab_active(Button const& btn, bool active) {
  if (!btn) {
    return;
  }
  btn.Background(SolidColorBrush(active ? Colors::SteelBlue() : Colors::Transparent()));
  btn.Foreground(SolidColorBrush(Colors::White()));
}

MenuFlyoutItem make_flyout(::winrt::hstring const& text) {
  MenuFlyoutItem item;
  item.Text(text);
  return item;
}

// Fixed Catalog(240)+Ambox(200)+Inspector(160) chrome needs ~1280x800 or the
// map SwapChainPanel collapses to tens of pixels (user sees empty slot).
constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 800;
constexpr int kMinWindowWidth = 1024;
constexpr int kMinWindowHeight = 700;

void ensure_usable_window_size(
    ::winrt::Microsoft::UI::Xaml::Window const& window,
    HWND native_hwnd) {
  try {
    auto app_window = window.AppWindow();
    if (app_window) {
      if (auto presenter =
              app_window.Presenter()
                  .try_as<::winrt::Microsoft::UI::Windowing::OverlappedPresenter>()) {
        presenter.PreferredMinimumWidth(kMinWindowWidth);
        presenter.PreferredMinimumHeight(kMinWindowHeight);
      }
      app_window.Resize({kInitialWindowWidth, kInitialWindowHeight});
    }
  } catch (::winrt::hresult_error const&) {
  }
  // WinUI 3 unpackaged often ignores AppWindow.Resize (screenshot showed
  // 512x320 with a 25px-tall map popup). Force the Win32 frame directly.
  if (native_hwnd && IsWindow(native_hwnd)) {
    SetWindowPos(native_hwnd, nullptr, 0, 0, kInitialWindowWidth,
                 kInitialWindowHeight,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

}  // namespace

MainWindow::MainWindow() {
  window_ = ::winrt::Microsoft::UI::Xaml::Window();
  window_.Title(L"SmartGIS WinUI");
  // Early attempt — may be ignored until Activate; activate() re-applies.
  ensure_usable_window_size(window_, nullptr);
  try {
    map_host_ = std::make_unique<MapHost>();
    session_ = content::MapContents::Create();
    // Do not StartRenderProcess here — it blocks up to ~30s on pipe/Hello and
    // freezes WinUI before the first paint (white client / Not Responding).
    build_chrome();
  } catch (::winrt::hresult_error const&) {
    TextBlock hint;
    hint.Text(L"SmartGIS WinUI (Windows App Runtime 1.7). "
              L"IDE chrome failed; map host skipped.");
    hint.Margin(Thickness{24, 24, 24, 24});
    hint.TextWrapping(::winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
    window_.Content(hint);
  }
}

MainWindow::~MainWindow() {
  // Stop deferred size heal before tearing down the map HWND / session.
  if (size_heal_timer_) {
    size_heal_timer_.Stop();
    size_heal_timer_ = nullptr;
  }
  if (render_thread_.joinable()) {
    render_thread_.join();
  }
  // Drop Xaml tree first so SizeChanged / Loaded / pointer handlers cannot
  // touch MapHost while we destroy child HWNDs (exit-path 0xC0000374).
  try {
    window_.Content(nullptr);
  } catch (::winrt::hresult_error const&) {
  }
  // Destroy MapHost (timers, child HWND, HostView users) before Shutdown
  // deletes MapWidgetHostViewImpl — same pattern as BrowserView teardown to
  // avoid late WM_TIMER / WM_PAINT heap corruption (STATUS_HEAP_CORRUPTION).
  map_host_.reset();
  if (session_) {
    session_->Shutdown();
    delete session_;
    session_ = nullptr;
  }
}

void MainWindow::activate() {
  window_.Activate();
  // Constructor Resize is often discarded for WinUI 3 unpackaged windows;
  // apply again after Activate so Catalog+Ambox leave a real map column.
  ensure_usable_window_size(window_, native_hwnd());
  set_status(L"Starting GPU…");
  content::MapContents* session = session_;
  if (render_thread_.joinable()) {
    render_thread_.join();
  }

  // Start GPU off the UI thread, but pump Win32 messages here until Hello
  // returns — then attach_map() synchronously. TryEnqueue-only attach raced
  // OnLaunched / --self-test (DispatcherQueue not drained → white slot /
  // view_id=0 / map-frame-fail).
  std::atomic<bool> gpu_done{false};
  std::atomic<bool> gpu_ok{false};
  render_thread_ = std::thread([session, &gpu_done, &gpu_ok]() {
    gpu_ok.store(session && session->StartRenderProcess());
    gpu_done.store(true);
  });

  while (!gpu_done.load()) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  if (render_thread_.joinable()) {
    render_thread_.join();
  }

  ensure_usable_window_size(window_, native_hwnd());
  attach_map();
  if (gpu_ok.load()) {
    set_status(L"Ready");
  } else {
    set_status(L"GPU failed — local MapScene only");
  }
  // WinUI unpackaged keeps restoring ~512x320 at 240 DPI; AppWindow.Resize and
  // a single SetWindowPos are overwritten. Pump + force-size until the frame
  // is usable so the owned map popup is not a 25px strip.
  {
    HWND hwnd = native_hwnd();
    for (int i = 0; i < 20; ++i) {
      if (hwnd && IsWindow(hwnd)) {
        SetWindowPos(hwnd, nullptr, 0, 0, kInitialWindowWidth,
                     kInitialWindowHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
      }
      MSG msg;
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      if (map_host_) {
        map_host_->sync_layout();
      }
      RECT wr = {};
      if (hwnd && GetWindowRect(hwnd, &wr)) {
        const int fw = wr.right - wr.left;
        const int fh = wr.bottom - wr.top;
        if (fw >= kMinWindowWidth && fh >= kMinWindowHeight) {
          break;
        }
      }
      Sleep(50);
    }
  }
  if (map_host_) {
    map_host_->sync_layout();
    if (HWND child = map_host_->map_child_hwnd()) {
      InvalidateRect(child, nullptr, FALSE);
      UpdateWindow(child);
    }
  }
  // After the XAML loop is alive, maximize / resize on the UI dispatcher.
  // A worker-thread SetWindowPos raced WinUI init and could tear down the
  // process; UI-thread AppWindow + SetWindowPos after a short timer is safe.
  {
    auto queue = window_.DispatcherQueue();
    size_heal_timer_ = queue.CreateTimer();
    size_heal_timer_.Interval(std::chrono::milliseconds(600));
    size_heal_timer_.IsRepeating(false);
    size_heal_timer_.Tick([this](auto&&, auto&&) {
      size_heal_timer_.Stop();
      HWND hwnd = native_hwnd();
      // Prefer a large restored frame — Maximize races some WinAppSDK builds.
      if (hwnd && IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        SetWindowPos(hwnd, nullptr, 40, 40, kInitialWindowWidth,
                     kInitialWindowHeight,
                     SWP_NOZORDER | SWP_SHOWWINDOW);
      }
      try {
        if (auto app_window = window_.AppWindow()) {
          app_window.Resize({kInitialWindowWidth, kInitialWindowHeight});
          if (auto presenter =
                  app_window.Presenter()
                      .try_as<::winrt::Microsoft::UI::Windowing::
                                  OverlappedPresenter>()) {
            presenter.PreferredMinimumWidth(kMinWindowWidth);
            presenter.PreferredMinimumHeight(kMinWindowHeight);
          }
        }
      } catch (::winrt::hresult_error const&) {
      }
      if (map_host_) {
        map_host_->sync_layout();
        if (HWND child = map_host_->map_child_hwnd()) {
          InvalidateRect(child, nullptr, FALSE);
        }
      }
    });
    size_heal_timer_.Start();
  }
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

bool MainWindow::has_ide_chrome() const {
  return menu_bar_ && catalog_panel_ && catalog_tree_ && ambox_panel_ &&
         map_column_ && tab_map_ && tab_data_ && tab_scene_ &&
         inspector_panel_ && insp_feature_ && insp_attrs_ && inspector_body_ &&
         status_bar_ && map_host_;
}

void MainWindow::set_status(std::wstring_view text) {
  if (status_bar_) {
    status_bar_.Text(::winrt::hstring{text});
  }
}

void MainWindow::highlight_map_tab(int index) {
  set_tab_active(tab_map_, index == 0);
  set_tab_active(tab_data_, index == 1);
  set_tab_active(tab_scene_, index == 2);
}

void MainWindow::select_map_tab(int index) {
  if (index < 0 || index > 2 || !map_host_) {
    return;
  }
  try {
    active_tab_ = index;
    highlight_map_tab(index);
    map_host_->set_map_surface_visible(true);
    content::ViewKind kind = content::ViewKind::kMapEdit;
    if (index == 1) {
      kind = content::ViewKind::kMapData;
    } else if (index == 2) {
      kind = content::ViewKind::kScene3d;
    }
    map_host_->show_kind(kind);
    map_host_->sync_layout();
    if (index == 2) {
      map_host_->set_active_tool("view3d.trackball");
      run_tool_command("view3d.trackball");
      set_status(L"3D");
    } else {
      map_host_->set_active_tool("view.pan");
      run_tool_command("view.pan");
      set_status(index == 0 ? L"Map Edit" : L"Data");
    }
  } catch (::winrt::hresult_error const&) {
    set_status(L"Tab switch ignored (invalid HWND)");
  }
}

bool MainWindow::run_tool_command(std::string_view command_id) {
  if (!session_ || !map_host_ || map_host_->view_id() == 0 ||
      command_id.empty()) {
    return false;
  }
  std::string id(command_id);
  if (id == "select" || id == "identify") {
    id = "selection.point";
  } else if (id == "pan") {
    id = "view.pan";
  }
  try {
    map_host_->set_active_tool(id);
    session_->ActivateTool(map_host_->view_id(), id.c_str());
    if (id == "view.backend.rhi" || id == "view.backend.maplibre") {
      session_->SetRenderBackend(id == "view.backend.maplibre" ? 1u : 0u);
    }
  } catch (::winrt::hresult_error const&) {
    return false;
  }
  if (id == "selection.clear") {
    set_status(L"Selection cleared");
  } else if (id == "edit.append.point") {
    set_status(L"Committed");
  } else if (id == "view.backend.maplibre") {
    set_status(L"Render: MapLibre (Track A)");
  } else if (id == "view.backend.rhi") {
    set_status(L"Render: RHI (Track B)");
  } else {
    std::wstring w(L"Activated ");
    w.append(id.begin(), id.end());
    set_status(w);
  }
  return true;
}

void MainWindow::on_open() {
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))) ||
      !dialog) {
    set_status(L"Open dialog unavailable");
    return;
  }
  if (SUCCEEDED(dialog->Show(native_hwnd()))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        char utf8[MAX_PATH * 4] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr,
                            nullptr);
        if (session_ && utf8[0]) {
          // Escape \ and " so Windows paths survive CatalogCall JSON.
          const std::string json =
              std::string("{\"op\":\"open\",\"path\":\"") +
              json_escape(utf8) + "\"}";
          session_->CatalogCall(json.c_str());
          // Views parity: load OGR vectors into MapScene for on-screen paint.
          if (map_host_) {
            map_host_->open_map_path(utf8);
          }
          // Best-effort: surface the opened path under Workspace/Maps.
          if (catalog_tree_ && catalog_tree_.RootNodes().Size() > 0) {
            auto root = catalog_tree_.RootNodes().GetAt(0);
            TreeViewNode maps{nullptr};
            for (uint32_t i = 0; i < root.Children().Size(); ++i) {
              auto child = root.Children().GetAt(i);
              if (auto boxed = child.Content().try_as<::winrt::hstring>()) {
                if (boxed == L"Maps") {
                  maps = child;
                  break;
                }
              }
            }
            if (maps) {
              TreeViewNode opened;
              opened.Content(::winrt::box_value(::winrt::hstring{path}));
              maps.Children().Append(opened);
            }
          }
          set_status(L"Opened");
        }
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
}

void MainWindow::on_exit() {
  ::winrt::Microsoft::UI::Xaml::Application::Current().Exit();
}

void MainWindow::wire_menu() {
  menu_bar_ = MenuBar();

  MenuBarItem file_menu;
  file_menu.Title(L"File");
  auto open = make_flyout(L"Open");
  open.Click([this](auto&&, auto&&) { on_open(); });
  file_menu.Items().Append(open);
  auto exit_item = make_flyout(L"Exit");
  exit_item.Click([this](auto&&, auto&&) { on_exit(); });
  file_menu.Items().Append(exit_item);
  menu_bar_.Items().Append(file_menu);

  MenuBarItem view_menu;
  view_menu.Title(L"View");
  auto map_item = make_flyout(L"Map Edit");
  map_item.Click([this](auto&&, auto&&) { select_map_tab(0); });
  view_menu.Items().Append(map_item);
  auto data_item = make_flyout(L"Data");
  data_item.Click([this](auto&&, auto&&) { select_map_tab(1); });
  view_menu.Items().Append(data_item);
  auto scene_item = make_flyout(L"3D");
  scene_item.Click([this](auto&&, auto&&) { select_map_tab(2); });
  view_menu.Items().Append(scene_item);
  auto rhi_item = make_flyout(L"RHI");
  rhi_item.Click([this](auto&&, auto&&) { run_tool_command("view.backend.rhi"); });
  view_menu.Items().Append(rhi_item);
  auto maplibre_item = make_flyout(L"MapLibre");
  maplibre_item.Click(
      [this](auto&&, auto&&) { run_tool_command("view.backend.maplibre"); });
  view_menu.Items().Append(maplibre_item);
  menu_bar_.Items().Append(view_menu);

  MenuBarItem tools_menu;
  tools_menu.Title(L"Tools");
  auto select = make_flyout(L"Select");
  select.Click([this](auto&&, auto&&) { run_tool_command("selection.point"); });
  tools_menu.Items().Append(select);
  auto draw = make_flyout(L"Draw");
  draw.Click([this](auto&&, auto&&) { run_tool_command("edit.append.point"); });
  tools_menu.Items().Append(draw);
  auto clear = make_flyout(L"Clear");
  clear.Click([this](auto&&, auto&&) { run_tool_command("selection.clear"); });
  tools_menu.Items().Append(clear);
  auto pan = make_flyout(L"Pan");
  pan.Click([this](auto&&, auto&&) { run_tool_command("view.pan"); });
  tools_menu.Items().Append(pan);
  menu_bar_.Items().Append(tools_menu);
}

void MainWindow::wire_catalog() {
  catalog_panel_ = Grid();
  RowDefinition title_row;
  title_row.Height(GridLength{0, GridUnitType::Auto});
  RowDefinition tree_row;
  tree_row.Height(GridLength{1, GridUnitType::Star});
  RowDefinition cmd_row;
  cmd_row.Height(GridLength{0, GridUnitType::Auto});
  catalog_panel_.RowDefinitions().Append(title_row);
  catalog_panel_.RowDefinitions().Append(tree_row);
  catalog_panel_.RowDefinitions().Append(cmd_row);
  catalog_panel_.Background(SolidColorBrush(Colors::DimGray()));
  catalog_panel_.Width(240);

  TextBlock title;
  title.Text(L"Catalog");
  title.Margin(Thickness{8, 8, 8, 4});
  title.Foreground(SolidColorBrush(Colors::White()));
  Grid::SetRow(title, 0);
  catalog_panel_.Children().Append(title);

  catalog_tree_ = TreeView();
  TreeViewNode root;
  root.Content(::winrt::box_value(L"Workspace"));
  TreeViewNode layers;
  layers.Content(::winrt::box_value(L"Layers"));
  root.Children().Append(layers);
  TreeViewNode maps;
  maps.Content(::winrt::box_value(L"Maps"));
  root.Children().Append(maps);
  catalog_tree_.RootNodes().Append(root);
  Grid::SetRow(catalog_tree_, 1);
  catalog_panel_.Children().Append(catalog_tree_);

  StackPanel cmds;
  cmds.Orientation(::winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal);
  cmds.Margin(Thickness{4, 4, 4, 4});
  Button refresh;
  refresh.Content(::winrt::box_value(L"Refresh"));
  refresh.Click([this](auto&&, auto&&) {
    if (session_) {
      session_->CatalogCall("{\"op\":\"refresh\"}");
    }
    set_status(L"Catalog refresh");
  });
  Button add_layer;
  add_layer.Content(::winrt::box_value(L"Add layer"));
  add_layer.Click([this](auto&&, auto&&) { on_open(); });
  cmds.Children().Append(refresh);
  cmds.Children().Append(add_layer);
  Grid::SetRow(cmds, 2);
  catalog_panel_.Children().Append(cmds);
}

void MainWindow::wire_ambox() {
  ambox_panel_ = StackPanel();
  ambox_panel_.Width(200);
  ambox_panel_.Background(SolidColorBrush(Colors::DimGray()));
  ambox_panel_.Padding(Thickness{8, 8, 8, 8});

  TextBlock title;
  title.Text(L"Ambox");
  title.Foreground(SolidColorBrush(Colors::White()));
  title.Margin(Thickness{0, 0, 0, 8});
  ambox_panel_.Children().Append(title);

  static constexpr const char* kBuiltins[] = {
      "selection.point", "selection.clear", "edit.append.point",
      "view.pan",        "view.zoom_in",    "view.zoom_out",
      "view3d.trackball",
  };
  for (const char* id : kBuiltins) {
    Button btn;
    std::wstring label(id, id + std::strlen(id));
    btn.Content(::winrt::box_value(::winrt::hstring{label}));
    btn.HorizontalAlignment(HorizontalAlignment::Stretch);
    btn.Margin(Thickness{0, 2, 0, 2});
    std::string cmd(id);
    btn.Click([this, cmd](auto&&, auto&&) { run_tool_command(cmd); });
    ambox_panel_.Children().Append(btn);
  }
}

void MainWindow::wire_inspector() {
  inspector_panel_ = Grid();
  RowDefinition tabs_row;
  tabs_row.Height(GridLength{0, GridUnitType::Auto});
  RowDefinition body_row;
  body_row.Height(GridLength{1, GridUnitType::Star});
  inspector_panel_.RowDefinitions().Append(tabs_row);
  inspector_panel_.RowDefinitions().Append(body_row);
  inspector_panel_.Background(SolidColorBrush(Colors::DarkSlateGray()));

  StackPanel tabs;
  tabs.Orientation(::winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal);
  insp_feature_ = make_tab_button(L"FeatureInfo");
  insp_attrs_ = make_tab_button(L"AttributeTable");
  insp_feature_.Click([this](auto&&, auto&&) {
    set_tab_active(insp_feature_, true);
    set_tab_active(insp_attrs_, false);
    if (inspector_body_) {
      inspector_body_.Text(L"FeatureInfo");
    }
  });
  insp_attrs_.Click([this](auto&&, auto&&) {
    set_tab_active(insp_feature_, false);
    set_tab_active(insp_attrs_, true);
    if (inspector_body_) {
      inspector_body_.Text(L"AttributeTable");
    }
  });
  tabs.Children().Append(insp_feature_);
  tabs.Children().Append(insp_attrs_);
  Grid::SetRow(tabs, 0);
  inspector_panel_.Children().Append(tabs);

  inspector_body_ = TextBlock();
  inspector_body_.Text(L"FeatureInfo");
  inspector_body_.Margin(Thickness{12, 8, 12, 8});
  inspector_body_.Foreground(SolidColorBrush(Colors::White()));
  Grid::SetRow(inspector_body_, 1);
  inspector_panel_.Children().Append(inspector_body_);
  set_tab_active(insp_feature_, true);
  set_tab_active(insp_attrs_, false);
}

void MainWindow::wire_map_tabs() {
  map_column_ = Grid();
  RowDefinition tab_row;
  tab_row.Height(GridLength{0, GridUnitType::Auto});
  RowDefinition map_row;
  map_row.Height(GridLength{1, GridUnitType::Star});
  map_column_.RowDefinitions().Append(tab_row);
  map_column_.RowDefinitions().Append(map_row);

  StackPanel tabs;
  tabs.Orientation(::winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal);
  tabs.Background(SolidColorBrush(Colors::Black()));
  tab_map_ = make_tab_button(L"Map Edit");
  tab_data_ = make_tab_button(L"Data");
  tab_scene_ = make_tab_button(L"3D");
  tab_map_.Click([this](auto&&, auto&&) { select_map_tab(0); });
  tab_data_.Click([this](auto&&, auto&&) { select_map_tab(1); });
  tab_scene_.Click([this](auto&&, auto&&) { select_map_tab(2); });
  tabs.Children().Append(tab_map_);
  tabs.Children().Append(tab_data_);
  tabs.Children().Append(tab_scene_);
  Grid::SetRow(tabs, 0);
  map_column_.Children().Append(tabs);

  auto map_el = map_host_->root_element();
  Grid::SetRow(map_el, 1);
  map_column_.Children().Append(map_el);
  highlight_map_tab(0);
}

void MainWindow::build_chrome() {
  root_ = Grid();
  RowDefinition menu_row;
  menu_row.Height(GridLength{0, GridUnitType::Auto});
  RowDefinition work_row;
  work_row.Height(GridLength{1, GridUnitType::Star});
  RowDefinition insp_row;
  insp_row.Height(GridLength{160, GridUnitType::Pixel});
  RowDefinition status_row;
  status_row.Height(GridLength{0, GridUnitType::Auto});
  root_.RowDefinitions().Append(menu_row);
  root_.RowDefinitions().Append(work_row);
  root_.RowDefinitions().Append(insp_row);
  root_.RowDefinitions().Append(status_row);
  root_.Background(SolidColorBrush(Colors::Black()));

  wire_menu();
  Grid::SetRow(menu_bar_, 0);
  root_.Children().Append(menu_bar_);

  Grid work;
  ColumnDefinition cat_col;
  cat_col.Width(GridLength{240, GridUnitType::Pixel});
  ColumnDefinition map_col;
  map_col.Width(GridLength{1, GridUnitType::Star});
  ColumnDefinition ambox_col;
  ambox_col.Width(GridLength{200, GridUnitType::Pixel});
  work.ColumnDefinitions().Append(cat_col);
  work.ColumnDefinitions().Append(map_col);
  work.ColumnDefinitions().Append(ambox_col);

  wire_catalog();
  Grid::SetColumn(catalog_panel_, 0);
  work.Children().Append(catalog_panel_);

  wire_map_tabs();
  // Keep a usable map column even when Catalog+Ambox take fixed pixels.
  map_column_.MinWidth(480);
  Grid::SetColumn(map_column_, 1);
  work.Children().Append(map_column_);

  wire_ambox();
  Grid::SetColumn(ambox_panel_, 2);
  work.Children().Append(ambox_panel_);

  Grid::SetRow(work, 1);
  root_.Children().Append(work);

  wire_inspector();
  Grid::SetRow(inspector_panel_, 2);
  root_.Children().Append(inspector_panel_);

  status_bar_ = TextBlock();
  status_bar_.Text(L"Ready");
  status_bar_.Margin(Thickness{8, 4, 8, 4});
  status_bar_.Foreground(SolidColorBrush(Colors::LightGray()));
  Grid::SetRow(status_bar_, 3);
  root_.Children().Append(status_bar_);

  root_.SizeChanged(
      [this](::winrt::Windows::Foundation::IInspectable const&,
             ::winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&) {
        if (map_host_) {
          map_host_->sync_layout();
        }
      });

  window_.Content(root_);
}

void MainWindow::attach_map() {
  if (!map_host_) {
    return;
  }
  map_host_->attach_session(session_, native_hwnd());
  window_.DispatcherQueue().TryEnqueue(
      winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal,
      [this]() {
        if (map_host_) {
          map_host_->sync_layout();
        }
      });
}

}  // namespace winui
}  // namespace app
