// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/map_host.h"

#include <d3d11.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <dxgi1_2.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>

namespace app {
namespace winui {
namespace {

constexpr wchar_t kChildClass[] = L"SmartGisWinuiMapHost";

void register_child_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MapHost::child_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kChildClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

MapHost::MapHost() {
  root_ = winrt::Microsoft::UI::Xaml::Controls::Grid();
  panel_ = winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel();
  status_ = winrt::Microsoft::UI::Xaml::Controls::TextBlock();
  status_.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
  status_.Margin(winrt::Microsoft::UI::Xaml::Thickness{12, 12, 12, 12});
  status_.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Top);
  status_.Foreground(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(
      winrt::Windows::UI::Colors::White()));

  root_.Children().Append(panel_);
  root_.Children().Append(status_);

  panel_.SizeChanged([this](winrt::Windows::Foundation::IInspectable const&,
                            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&) {
    sync_layout();
  });
}

MapHost::~MapHost() {
  destroy_child_hwnd();
}

winrt::Microsoft::UI::Xaml::UIElement MapHost::root_element() const {
  return root_;
}

void MapHost::attach_session(content::MapContents* session, HWND window_hwnd) {
  session_ = session;
  window_hwnd_ = window_hwnd;
  if (!session_) {
    return;
  }
  view_id_ = session_->OpenView(content::ViewKind::kMapEdit);
  view_ = session_->AttachSurface(view_id_,
                                   content::PresentMode::kSharedTexture);
  if (!try_attach_swap_chain()) {
    attach_child_hwnd();
  }
  sync_layout();
}

bool MapHost::try_attach_swap_chain() {
  if (!view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  if (!surface.nt_handle) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  HRESULT hr = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
  if (FAILED(hr)) {
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  hr = device.As(&dxgi_device);
  if (FAILED(hr)) {
    return false;
  }
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  hr = dxgi_device->GetAdapter(&adapter);
  if (FAILED(hr)) {
    return false;
  }
  Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
  hr = adapter->GetParent(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    return false;
  }

  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = surface.width_px ? surface.width_px : 1;
  desc.Height = surface.height_px ? surface.height_px : 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap;
  hr = factory->CreateSwapChainForComposition(device.Get(), &desc, nullptr,
                                              &swap);
  if (FAILED(hr)) {
    return false;
  }

  auto native = panel_.try_as<ISwapChainPanelNative>();
  if (!native) {
    return false;
  }
  hr = native->SetSwapChain(swap.Get());
  if (FAILED(hr)) {
    return false;
  }

  view_->SetPresentMode(content::PresentMode::kSharedTexture);
  swap_chain_ = true;
  destroy_child_hwnd();
  status_.Text(L"Present: SwapChainPanel (DXGI shared handle)");
  return true;
}

void MapHost::attach_child_hwnd() {
  if (!window_hwnd_) {
    return;
  }
  register_child_class();
  if (!child_hwnd_) {
    child_hwnd_ = CreateWindowExW(
        0, kChildClass, L"Map", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0,
        1, 1, window_hwnd_, nullptr, GetModuleHandleW(nullptr), this);
  }
  if (view_) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = child_hwnd_;
    view_->Create(params, content::MapWidgetHostView::Preferences());
    view_->SetPresentMode(content::PresentMode::kChildHwnd);
  }
  swap_chain_ = false;
  wchar_t text[256];
  swprintf_s(text, L"Present: HWND island  |  %s", process_path());
  status_.Text(text);
}

void MapHost::destroy_child_hwnd() {
  if (child_hwnd_) {
    DestroyWindow(child_hwnd_);
    child_hwnd_ = nullptr;
  }
}

void MapHost::sync_layout() {
  if (!child_hwnd_ || !window_hwnd_ || !panel_) {
    return;
  }
  const float w = static_cast<float>(panel_.ActualWidth());
  const float h = static_cast<float>(panel_.ActualHeight());
  if (w < 1.f || h < 1.f) {
    return;
  }
  const auto transform = panel_.TransformToVisual(nullptr);
  const auto origin = transform.TransformPoint({0.f, 0.f});
  const UINT dpi = GetDpiForWindow(window_hwnd_);
  const float scale = dpi > 0 ? static_cast<float>(dpi) / 96.f : 1.f;
  const int x = static_cast<int>(origin.X * scale);
  const int y = static_cast<int>(origin.Y * scale);
  const int pw = static_cast<int>(w * scale);
  const int ph = static_cast<int>(h * scale);
  SetWindowPos(child_hwnd_, HWND_TOP, x, y, pw, ph,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  if (view_) {
    view_->Resize(pw, ph, scale * 96.f);
  }
}

const wchar_t* MapHost::present_path() const {
  return swap_chain_ ? L"SwapChainPanel" : L"HWND island";
}

const wchar_t* MapHost::process_path() const {
  if (session_ && session_->IsOopRender()) {
    return L"OOP SmartGisRender.exe";
  }
  return L"in-process LoadLibrary fallback";
}

void MapHost::paint_child() const {
  if (!child_hwnd_) {
    return;
  }
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(child_hwnd_, &ps);
  RECT rc;
  GetClientRect(child_hwnd_, &rc);
  const HBRUSH brush = CreateSolidBrush(RGB(28, 42, 58));
  FillRect(hdc, &rc, brush);
  DeleteObject(brush);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(230, 236, 242));
  const wchar_t* line1 = L"SmartGIS map (WinUI host)";
  DrawTextW(hdc, line1, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  RECT rc2 = rc;
  rc2.top += 28;
  DrawTextW(hdc, process_path(), -1, &rc2,
            DT_CENTER | DT_TOP | DT_SINGLELINE);
  EndPaint(child_hwnd_, &ps);
}

LRESULT CALLBACK MapHost::child_wnd_proc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam) {
  MapHost* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<MapHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<MapHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self && self->session_ && self->view_id_ != 0) {
    content::InputEvent ev{};
    ev.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
    ev.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
    bool dispatch = false;
    switch (msg) {
      case WM_MOUSEMOVE:
        ev.kind = content::InputEvent::Kind::kMouseMove;
        dispatch = true;
        break;
      case WM_LBUTTONDOWN:
        ev.kind = content::InputEvent::Kind::kLDown;
        dispatch = true;
        break;
      case WM_LBUTTONUP:
        ev.kind = content::InputEvent::Kind::kLUp;
        dispatch = true;
        break;
      case WM_RBUTTONDOWN:
        ev.kind = content::InputEvent::Kind::kRDown;
        dispatch = true;
        break;
      case WM_RBUTTONUP:
        ev.kind = content::InputEvent::Kind::kRUp;
        dispatch = true;
        break;
      case WM_MOUSEWHEEL:
        ev.kind = content::InputEvent::Kind::kWheel;
        ev.wheel = GET_WHEEL_DELTA_WPARAM(wparam);
        dispatch = true;
        break;
      default:
        break;
    }
    if (dispatch) {
      self->session_->Dispatch(self->view_id_, ev);
    }
  }

  if (msg == WM_PAINT && self) {
    self->paint_child();
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace winui
}  // namespace app
