// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

#include "base/core/log.h"

using namespace base;

namespace scenic {
namespace detail {

// Defined in draw.cpp — screen-label sidecar (no class size change).
void release_sprite_sidecar(D3dRenderDevice* device);

D3dRenderDevice::D3dRenderDevice()
    : hwnd_(nullptr),
      backbuffer_width_(0),
      backbuffer_height_(0),
      device_(nullptr),
      context_(nullptr),
      swapchain_(nullptr),
      rtv_(nullptr),
      dsv_(nullptr),
      depth_tex_(nullptr),
      color_tex_(nullptr),
      capture_tex_(nullptr),
      mesh_vs_(nullptr),
      mesh_ps_(nullptr),
      mesh_il_(nullptr),
      mesh_cb_(nullptr),
      mesh_rs_(nullptr),
      mesh_dss_(nullptr),
      mesh_bs_(nullptr),
      mesh_pipeline_ok_(false),
      state_manager_(std::make_unique<D3dGpuStateManager>()),
      device_caps_(nullptr),
      clear_depth_(1.f),
      clear_stencil_(0),
      modelview_stack_(std::make_unique<Matrix[]>(kMatrixStackMax)),
      projection_stack_(std::make_unique<Matrix[]>(kMatrixStackMax)),
      modelview_sp_(0),
      projection_sp_(0) {
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
  projection_.identity();
}

D3dRenderDevice::D3dRenderDevice(HINSTANCE hDLL)
    : hwnd_(nullptr),
      backbuffer_width_(0),
      backbuffer_height_(0),
      device_(nullptr),
      context_(nullptr),
      swapchain_(nullptr),
      rtv_(nullptr),
      dsv_(nullptr),
      depth_tex_(nullptr),
      color_tex_(nullptr),
      capture_tex_(nullptr),
      mesh_vs_(nullptr),
      mesh_ps_(nullptr),
      mesh_il_(nullptr),
      mesh_cb_(nullptr),
      mesh_rs_(nullptr),
      mesh_dss_(nullptr),
      mesh_bs_(nullptr),
      mesh_pipeline_ok_(false),
      state_manager_(std::make_unique<D3dGpuStateManager>()),
      device_caps_(nullptr),
      clear_depth_(1.f),
      clear_stencil_(0),
      modelview_stack_(std::make_unique<Matrix[]>(kMatrixStackMax)),
      projection_stack_(std::make_unique<Matrix[]>(kMatrixStackMax)),
      modelview_sp_(0),
      projection_sp_(0) {
  m_rBaseApi = RA_D3D09;
  m_hDLL = hDLL;
  m_matrixMode = MM_MODELVIEW;
  clear_color_[0] = 1.f;
  clear_color_[1] = 0.f;
  clear_color_[2] = 0.f;
  clear_color_[3] = 1.f;
  modelview_.identity();
  projection_.identity();
  projection_.identity();
}

D3dRenderDevice::~D3dRenderDevice() { Release(); }

Matrix& D3dRenderDevice::active_matrix() {
  return (m_matrixMode == MM_PROJECTION) ? projection_ : modelview_;
}

const Matrix& D3dRenderDevice::active_matrix() const {
  return (m_matrixMode == MM_PROJECTION) ? projection_ : modelview_;
}

void D3dRenderDevice::release_targets() {
  if (context_) {
    context_->OMSetRenderTargets(0, nullptr, nullptr);
  }
  safe_release(rtv_);
  safe_release(dsv_);
  safe_release(depth_tex_);
  safe_release(color_tex_);
  // Keep capture_tex_ — SwapBuffers recreates when size changes.
}

long D3dRenderDevice::create_swapchain_and_targets() {
  if (!device_ || !hwnd_) return kErrFailure;

  RECT rc = {};
  ::GetClientRect(hwnd_, &rc);
  UINT width =
      static_cast<UINT>((rc.right > rc.left) ? (rc.right - rc.left) : 1);
  UINT height =
      static_cast<UINT>((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 1);

  IDXGIDevice* dxgi_device = nullptr;
  HRESULT hr = device_->QueryInterface(__uuidof(IDXGIDevice),
                                       reinterpret_cast<void**>(&dxgi_device));
  if (FAILED(hr) || !dxgi_device) return kErrFailure;

  IDXGIAdapter* adapter = nullptr;
  hr = dxgi_device->GetAdapter(&adapter);
  safe_release(dxgi_device);
  if (FAILED(hr) || !adapter) return kErrFailure;

  IDXGIFactory* factory = nullptr;
  hr = adapter->GetParent(__uuidof(IDXGIFactory),
                          reinterpret_cast<void**>(&factory));
  safe_release(adapter);
  if (FAILED(hr) || !factory) return kErrFailure;

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
  if (FAILED(hr) || !swapchain_) return kErrFailure;

  backbuffer_width_ = width;
  backbuffer_height_ = height;
  // Fresh swapchain — bind views without ResizeBuffers.
  return resize_targets(width, height, /*resize_buffers=*/false);
}

long D3dRenderDevice::resize_targets(UINT width, UINT height,
                                        bool resize_buffers) {
  if (!device_ || !context_ || !swapchain_) return kErrFailure;
  if (width == 0) width = 1;
  if (height == 0) height = 1;

  release_targets();

  if (resize_buffers) {
    HRESULT hr =
        swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return kErrFailure;
  }

  D3D11_TEXTURE2D_DESC color_desc = {};
  color_desc.Width = width;
  color_desc.Height = height;
  color_desc.MipLevels = 1;
  color_desc.ArraySize = 1;
  color_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  color_desc.SampleDesc.Count = 1;
  color_desc.Usage = D3D11_USAGE_DEFAULT;
  color_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  HRESULT hr = device_->CreateTexture2D(&color_desc, nullptr, &color_tex_);
  if (FAILED(hr) || !color_tex_) return kErrFailure;

  hr = device_->CreateRenderTargetView(color_tex_, nullptr, &rtv_);
  if (FAILED(hr) || !rtv_) return kErrFailure;

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
  if (FAILED(hr) || !depth_tex_) return kErrFailure;

  hr = device_->CreateDepthStencilView(depth_tex_, nullptr, &dsv_);
  if (FAILED(hr) || !dsv_) return kErrFailure;

  context_->OMSetRenderTargets(1, &rtv_, dsv_);

  D3D11_VIEWPORT vp = {};
  vp.Width = static_cast<float>(width);
  vp.Height = static_cast<float>(height);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;
  context_->RSSetViewports(1, &vp);

  backbuffer_width_ = width;
  backbuffer_height_ = height;
  return kErrNone;
}

long D3dRenderDevice::Init(HWND hWnd, const char* logname) {
  if (!::IsWindow(hWnd)) return kErrFailure;
  hwnd_ = hWnd;

  m_strLogName = logname ? logname : "";

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  // Skip D3D11 debug layer in leftover path — it interacts badly with
  // abrupt TerminateProcess teardown used by showcases.
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
    return kErrFailure;
  }

  if (kErrNone != create_swapchain_and_targets()) {
    Destroy();
    return kErrFailure;
  }

  device_caps_ = std::make_unique<D3dDeviceCaps>(this);

  Viewport3D viewport;
  viewport.ulX = 0;
  viewport.ulY = 0;
  viewport.ulWidth = backbuffer_width_;
  viewport.ulHeight = backbuffer_height_;
  viewport.fZNear = 0.1f;
  viewport.fZFar = 1000.f;
  viewport.fFovy = 45.f;
  SetViewport(viewport);

  SetClearColor(Color(1, 0, 0));
  SetDepthClearValue(1.0f);
  SetStencilClearValue(0);

  if (ensure_mesh_pipeline() != kErrNone) {
    LOGGING(LOG_ERROR, "D3D11 mesh pipeline compile failed");
    Destroy();
    return kErrFailure;
  }

  LOGGING(LOG_INFO, "Init D3D11 D3dRenderDevice ok (FL=0x%x)",
          static_cast<unsigned>(got));
  return kErrNone;
}

uint D3dRenderDevice::alloc_texture_handle() {
  return next_texture_handle_++;
}

void D3dRenderDevice::release_gpu_texture(D3dGpuTexture& gpu) {
  safe_release(gpu.srv);
  safe_release(gpu.rtv);
  safe_release(gpu.dsv);
  safe_release(gpu.tex);
  gpu.format = DXGI_FORMAT_UNKNOWN;
  gpu.width = 0;
  gpu.height = 0;
}

void D3dRenderDevice::release_gpu_resources() {
  bound_texture_ = nullptr;
  bound_fbo_handle_ = 0;
  for (auto& kv : gpu_textures_) {
    release_gpu_texture(kv.second);
  }
  gpu_textures_.clear();
  gpu_fbos_.clear();
  for (auto& kv : gpu_renderbuffers_) {
    release_gpu_texture(kv.second);
  }
  gpu_renderbuffers_.clear();
  for (D3dFontSlot& slot : fonts_) {
    if (slot.font) {
      ::DeleteObject(slot.font);
      slot.font = nullptr;
    }
  }
  fonts_.clear();
  safe_release(linear_sampler_);
}

ID3D11ShaderResourceView* D3dRenderDevice::texture_srv(uint handle) const {
  auto it = gpu_textures_.find(handle);
  if (it == gpu_textures_.end()) {
    return nullptr;
  }
  return it->second.srv;
}

ID3D11SamplerState* D3dRenderDevice::linear_sampler() {
  if (linear_sampler_) {
    return linear_sampler_;
  }
  if (!device_) {
    return nullptr;
  }
  D3D11_SAMPLER_DESC sd = {};
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  if (FAILED(device_->CreateSamplerState(&sd, &linear_sampler_))) {
    return nullptr;
  }
  return linear_sampler_;
}

long D3dRenderDevice::Destroy() {
  for (D3dDeferredSlot& s : deferred_slots_) {
    safe_release(s.list);
    safe_release(s.mesh_cb);
    safe_release(s.ctx);
  }
  deferred_slots_.clear();
  deferred_recording_ = false;
  release_mesh_pipeline();
  release_sprite_sidecar(this);
  release_gpu_resources();
  release_targets();
  safe_release(swapchain_);
  if (context_) {
    context_->ClearState();
    context_->Flush();
  }
  safe_release(context_);
  safe_release(device_);
  hwnd_ = nullptr;
  return kErrNone;
}

long D3dRenderDevice::Release() {
  Destroy();
  device_caps_.reset();
  state_manager_.reset();
  modelview_stack_.reset();
  projection_stack_.reset();
  return kErrNone;
}

GpuStateManager* D3dRenderDevice::GetStateManager() {
  return state_manager_.get();
}

DeviceCaps3d* D3dRenderDevice::GetDeviceCaps() {
  return device_caps_.get();
}

}  // namespace detail
}  // namespace scenic
