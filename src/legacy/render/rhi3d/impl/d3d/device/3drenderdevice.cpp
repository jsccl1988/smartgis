// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/device/3drenderdevice.h"

#include "base/core/log.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

using namespace base;

namespace render {

SmtD3DRenderDevice::SmtD3DRenderDevice()
    : hwnd_(nullptr),
      backbuffer_width_(0),
      backbuffer_height_(0),
      device_(nullptr),
      context_(nullptr),
      swapchain_(nullptr),
      rtv_(nullptr),
      dsv_(nullptr),
      depth_tex_(nullptr),
      state_manager_(new SmtD3DGPUStateManager()),
      device_caps_(nullptr),
      clear_depth_(1.f),
      clear_stencil_(0) {
  // Leftover enum still named RA_D3D09 (historical D3D9 slot); impl is D3D11.
  m_rBaseApi = RA_D3D09;
  m_hDLL = nullptr;
  m_matrixMode = MM_MODELVIEW;
  clear_color_[0] = 1.f;
  clear_color_[1] = 0.f;
  clear_color_[2] = 0.f;
  clear_color_[3] = 1.f;
  modelview_.identity();
  projection_.identity();
}

SmtD3DRenderDevice::SmtD3DRenderDevice(HINSTANCE hDLL)
    : hwnd_(nullptr),
      backbuffer_width_(0),
      backbuffer_height_(0),
      device_(nullptr),
      context_(nullptr),
      swapchain_(nullptr),
      rtv_(nullptr),
      dsv_(nullptr),
      depth_tex_(nullptr),
      state_manager_(new SmtD3DGPUStateManager()),
      device_caps_(nullptr),
      clear_depth_(1.f),
      clear_stencil_(0) {
  m_rBaseApi = RA_D3D09;
  m_hDLL = hDLL;
  m_matrixMode = MM_MODELVIEW;
  clear_color_[0] = 1.f;
  clear_color_[1] = 0.f;
  clear_color_[2] = 0.f;
  clear_color_[3] = 1.f;
  modelview_.identity();
  projection_.identity();
}

SmtD3DRenderDevice::~SmtD3DRenderDevice() { Release(); }

Matrix& SmtD3DRenderDevice::active_matrix() {
  return (m_matrixMode == MM_PROJECTION) ? projection_ : modelview_;
}

const Matrix& SmtD3DRenderDevice::active_matrix() const {
  return (m_matrixMode == MM_PROJECTION) ? projection_ : modelview_;
}

void SmtD3DRenderDevice::release_targets() {
  if (context_) {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
  }
  safe_release(rtv_);
  safe_release(dsv_);
  safe_release(depth_tex_);
}

long SmtD3DRenderDevice::create_swapchain_and_targets() {
  if (!device_ || !hwnd_) return SMT_ERR_FAILURE;

  RECT rc = {};
  ::GetClientRect(hwnd_, &rc);
  UINT width =
      static_cast<UINT>((rc.right > rc.left) ? (rc.right - rc.left) : 1);
  UINT height =
      static_cast<UINT>((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 1);

  IDXGIDevice* dxgi_device = nullptr;
  HRESULT hr = device_->QueryInterface(__uuidof(IDXGIDevice),
                                       reinterpret_cast<void**>(&dxgi_device));
  if (FAILED(hr) || !dxgi_device) return SMT_ERR_FAILURE;

  IDXGIAdapter* adapter = nullptr;
  hr = dxgi_device->GetAdapter(&adapter);
  safe_release(dxgi_device);
  if (FAILED(hr) || !adapter) return SMT_ERR_FAILURE;

  IDXGIFactory* factory = nullptr;
  hr = adapter->GetParent(__uuidof(IDXGIFactory),
                          reinterpret_cast<void**>(&factory));
  safe_release(adapter);
  if (FAILED(hr) || !factory) return SMT_ERR_FAILURE;

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = width;
  sd.BufferDesc.Height = height;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd_;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  hr = factory->CreateSwapChain(device_, &sd, &swapchain_);
  factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
  safe_release(factory);
  if (FAILED(hr) || !swapchain_) return SMT_ERR_FAILURE;

  backbuffer_width_ = width;
  backbuffer_height_ = height;
  // Fresh swapchain — bind views without ResizeBuffers.
  return resize_targets(width, height, /*resize_buffers=*/false);
}

long SmtD3DRenderDevice::resize_targets(UINT width, UINT height,
                                        bool resize_buffers) {
  if (!device_ || !context_ || !swapchain_) return SMT_ERR_FAILURE;
  if (width == 0) width = 1;
  if (height == 0) height = 1;

  release_targets();

  if (resize_buffers) {
    HRESULT hr =
        swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return SMT_ERR_FAILURE;
  }

  ID3D11Texture2D* backbuffer = nullptr;
  HRESULT hr = swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                     reinterpret_cast<void**>(&backbuffer));
  if (FAILED(hr) || !backbuffer) return SMT_ERR_FAILURE;

  hr = device_->CreateRenderTargetView(backbuffer, nullptr, &rtv_);
  safe_release(backbuffer);
  if (FAILED(hr) || !rtv_) return SMT_ERR_FAILURE;

  D3D11_TEXTURE2D_DESC depth_desc = {};
  depth_desc.Width = width;
  depth_desc.Height = height;
  depth_desc.MipLevels = 1;
  depth_desc.ArraySize = 1;
  depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_desc.SampleDesc.Count = 1;
  depth_desc.Usage = D3D11_USAGE_DEFAULT;
  depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  hr = device_->CreateTexture2D(&depth_desc, nullptr, &depth_tex_);
  if (FAILED(hr) || !depth_tex_) return SMT_ERR_FAILURE;

  hr = device_->CreateDepthStencilView(depth_tex_, nullptr, &dsv_);
  if (FAILED(hr) || !dsv_) return SMT_ERR_FAILURE;

  context_->OMSetRenderTargets(1, &rtv_, dsv_);

  D3D11_VIEWPORT vp = {};
  vp.Width = static_cast<float>(width);
  vp.Height = static_cast<float>(height);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;
  context_->RSSetViewports(1, &vp);

  backbuffer_width_ = width;
  backbuffer_height_ = height;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::Init(HWND hWnd, const char* logname) {
  if (!::IsWindow(hWnd)) return SMT_ERR_FAILURE;
  hwnd_ = hWnd;
  // Present strangler: record HWND into leftover_session (Null). D3D owns
  // Present.
  bind_rhi_present(hWnd);

  m_strLogName = logname ? logname : "";

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
  };
  D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;

  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                 flags, levels, ARRAYSIZE(levels),
                                 D3D11_SDK_VERSION, &device_, &got, &context_);
  // Retry without DEBUG if the debug layer is missing.
  if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
    flags &= ~D3D11_CREATE_DEVICE_DEBUG;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                           levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                           &device_, &got, &context_);
  }
  if (FAILED(hr) || !device_ || !context_) {
    LOGGING(LOG_ERROR, "D3D11CreateDevice failed (hr=0x%08lx)",
            static_cast<unsigned long>(hr));
    return SMT_ERR_FAILURE;
  }

  if (SMT_ERR_NONE != create_swapchain_and_targets()) {
    Destroy();
    return SMT_ERR_FAILURE;
  }

  delete device_caps_;
  device_caps_ = new SmtD3DDeviceCaps(this);

  Viewport3D viewport;
  viewport.ulX = 0;
  viewport.ulY = 0;
  viewport.ulWidth = backbuffer_width_;
  viewport.ulHeight = backbuffer_height_;
  viewport.fZNear = 0.1f;
  viewport.fZFar = 1000.f;
  viewport.fFovy = 45.f;
  SetViewport(viewport);

  SetClearColor(SmtColor(1, 0, 0));
  SetDepthClearValue(1.0f);
  SetStencilClearValue(0);

  LOGGING(LOG_INFO, "Init D3D11 SmtD3DRenderDevice ok (FL=0x%x)",
          static_cast<unsigned>(got));
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::Destroy() {
  release_targets();
  safe_release(swapchain_);
  if (context_) {
    context_->ClearState();
    context_->Flush();
  }
  safe_release(context_);
  safe_release(device_);
  hwnd_ = nullptr;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::Release() {
  Destroy();
  delete device_caps_;
  device_caps_ = nullptr;
  delete state_manager_;
  state_manager_ = nullptr;
  return SMT_ERR_NONE;
}

SmtGPUStateManager* SmtD3DRenderDevice::GetStateManager() {
  return state_manager_;
}

Smt3DDeviceCaps* SmtD3DRenderDevice::GetDeviceCaps() { return device_caps_; }

}  // namespace render
