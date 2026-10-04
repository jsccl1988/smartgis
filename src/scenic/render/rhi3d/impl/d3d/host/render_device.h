// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/impl/d3d/caps/device_caps.h"
#include "scenic/render/rhi3d/impl/d3d/host/deferred_draw.h"
#include "scenic/render/rhi3d/impl/d3d/paint/states_manager.h"
#include "scenic/render/rhi3d/impl/d3d/prerequisites.h"
#include "scenic/render/rhi3d/public/device/render_device.h"

namespace scenic {
namespace detail {

// Leftover RenderDevice3d backed by D3D11 (not D3DX / D3D9).
// Layout: host/ (Init/Present), resource/ (VB·IB·texture·FBO·font),
// paint/ (draw/matrix/state). HWND present stays on D3D11 swapchain.
class SCENIC_RENDER_D3D_EXPORT D3dRenderDevice : public RenderDevice3d {
 public:
  D3dRenderDevice();
  explicit D3dRenderDevice(HINSTANCE hDLL);
  ~D3dRenderDevice() override;

  long Init(HWND hWnd, const char* logname) override;
  long Release() override;
  long Destroy() override;

  GpuStateManager* GetStateManager() override;
  DeviceCaps3d* GetDeviceCaps() override;

  long BeginRender() override;
  long EndRender() override;

  long DrawPrimitives(PrimitiveType type, VertexBuffer* pVB,
                      ulong baseVertex, ulong primitiveCount) override;
  long DrawIndexedPrimitives(PrimitiveType type, VertexBuffer* pVB,
                             IndexBuffer* pIB, ulong baseIndex,
                             ulong primitiveCount) override;

  long DrawText(uint unID, float xpos, float ypos, float zpos,
                const Color& color, const char*, ...) override;
  long DrawText(uint nID, float xscreen, float yscreen, const Color& color,
                const char*, ...) override;
  // Screen-space BGRA sprite (center in top-down window pixels).
  long DrawScreenBgra(float cx, float cy, int w, int h,
                      const unsigned char* bgra);

  long SwapBuffers() override;

  long SetClearColor(const Color& clr) override;
  long SetDepthClearValue(float z) override;
  long SetStencilClearValue(ulong s) override;
  long Clear(ulong flags) override;

  long MatrixModeSet(MatrixMode mode) override;
  MatrixMode MatrixModeGet() const override;
  long MatrixLoadIdentity() override;
  long MatrixLoad(const Matrix& m) override;
  long MatrixPush() override;
  long MatrixPop() override;
  long MatrixScale(float x, float y, float z) override;
  long MatrixTranslation(float x, float y, float z) override;
  long MatrixRotation(float angle, float x, float y, float z) override;
  long MatrixMultiply(const Matrix& m) override;
  Matrix MatrixGet() override;

  long GetFrustum(Frustum& frustum) override;

  long SetViewport(Viewport3D& viewport) override;
  Viewport3D& GetViewport(void) override { return m_viewPort; }

  long SetOrtho(float left, float right, float bottom, float top, float zNear,
                float zFar) override;
  long SetPerspective(float fovy, float aspect, float zNear,
                      float zFar) override;
  long SetViewLookAt(Vector3& vPos, Vector3& vView, Vector3& vUp) override;

  long Transform2DTo3D(Vector3& vOrg, Vector3& vTar,
                       const lPoint& point) override;
  long Transform3DTo2D(const Vector3& ver3D, lPoint& point) override;

  long SetBlending(bool bBlending) override;
  long SetBackfaceCulling(RenderStateValue rsv) override;
  long SetStencilBufferMode(RenderStateValue rsv, ulong ul) override;
  long SetDepthBufferMode(RenderStateValue rsv) override;
  long SetShadeMode(RenderStateValue rsv, float, const Color& clr) override;

  long SetLight(int index, Light* pLight) override;
  long SetAmbientLight(const Color& clr) override;
  long SetTexture(Texture* pTex) override;
  long SetMaterial(Material* pMat) override;
  long SetFog(FogMode mode, const Color& color, float density, float start,
              float end) override;

  VertexBuffer* CreateVertexBuffer(int nCount, ulong ulFormat,
                                      bool bDynamic = true) override;
  IndexBuffer* CreateIndexBuffer(int nCount) override;

  VideoBuffer* CreateVideoBuffer(ArrayType type) override;
  long DestroyBuffer(VideoBuffer* buffer) override;
  long DestroyIndexBuffer(VideoBuffer* buffer) override;

  long SetVertexArray(int components, Type type, int stride,
                      void* data) override;
  long SetTextureCoordsArray(int components, Type type, int stride,
                             void* data) override;
  long SetNormalArray(Type type, int stride, void* data) override;
  long SetIndexArray(Type type, int stride, void* data) override;
  long EnableArray(ArrayType type, bool enabled) override;

  long BindBuffer(VideoBuffer* buffer) override;
  long BindIndexBuffer(VideoBuffer* buffer) override;
  long UnbindBuffer() override;
  long UnbindIndexBuffer() override;
  long UpdateBuffer(VideoBuffer* buffer, void* data, uint size,
                    VideoBufferStoreMethod method) override;
  long UpdateIndexBuffer(VideoBuffer* buffer, void* data, uint size,
                         VideoBufferStoreMethod method) override;
  void* MapBuffer(VideoBuffer* buffer, AccessMode access) override;
  long UnmapBuffer(VideoBuffer* buffer) override;
  void* MapIndexBuffer(VideoBuffer* buffer, AccessMode access) override;
  long UnmapIndexBuffer(VideoBuffer* buffer) override;

  Shader* CreateVertexShader(const char* szName) override;
  Shader* CreatePixelShader(const char* szName) override;
  long DestroyShader(const char* szName) override;
  Shader* GetShader(const char* szName) override;

  Program* CreateProgram(const char* szName) override;
  long DestroyProgram(const char* szName) override;
  Program* GetProgram(const char* szName) override;

  long LoadShaderSource(Shader* shader, char* source) override;
  long CompileShader(Shader* shader) override;
  long IsShaderCompiled(Shader* shader) override;
  char* GetShaderLog(Shader* shader) override;

  long BindProgram(Program* program) override;
  long UnbindProgram() override;
  long SetProgramVertexShader(Program* program, Shader* shader) override;
  long SetProgramPixelShader(Program* program, Shader* shader) override;
  long LinkProgram(Program* program) override;
  long IsProgramLinked(Program* program) override;
  char* GetProgramLinkLog(Program* program) override;

  long SetProgramVector(Program* program, string& param,
                        const Vector4& value) override;
  long SetProgramVector(Program* program, string& param,
                        const Vector3& value) override;
  long SetProgramVector(Program* program, string& param,
                        const Vector2& value) override;
  long SetProgramFloat(Program* program, string& param,
                       float value) override;
  long SetProgramInt(Program* program, string& param, int value) override;
  long GetProgramFloat(Program* program, string& param,
                       float* value) override;
  long SetProgramTexture(Program* program, string& param,
                         int texture) override;

  Texture* CreateTexture(const char* szName) override;
  long DestroyTexture(const char* szName) override;
  Texture* GetTexture(const char* szName) override;

  long GenerateMipmap(Texture* texture) override;
  long BindTexture(Texture* texture) override;
  long BuildTexture(Texture* texture) override;
  long BindRectTexture(Texture* texture) override;
  long UnbindTexture() override;
  long UnbindRectTexture(void) override;

  FrameBuffer* CreateFrameBuffer() override;
  long DestroyFrameBuffer(FrameBuffer* frameBuffer) override;
  long BindFrameBuffer(FrameBuffer* frameBuffer) override;
  long UnbindFrameBuffer() override;
  RenderBuffer* CreateRenderBuffer(TextureFormat format, uint width,
                                      uint height) override;
  long DestroyRenderBuffer(RenderBuffer* renderBuffer) override;
  long AttachRenderBuffer(FrameBuffer* frameBuffer,
                          RenderBuffer* renderBuffer,
                          RenderBufferSlot slot) override;
  long AttachTexture(FrameBuffer* frameBuffer, Texture* texture2D,
                     RenderBufferSlot slot) override;
  FrameBufferStatus CheckFrameBufferStatus() override;

  long CreateFont(const char* szChType, int nHeight, int nWidth, int nWeight,
                  bool bItalic, bool bUnderline, bool bStrike, ulong dwSize,
                  uint& unID) override;

  long DrawCube3D(Vector3 vCenter, float fWidth, Color color) override;

  // Read presented backbuffer as tightly packed BGR24 (bottom-up, BMP order).
  long CaptureBgr24(unsigned char* out_bgr24, int width_px, int height_px);

  ID3D11Device* device() const { return device_; }
  ID3D11DeviceContext* context() const { return context_; }
  // Immediate or TLS-bound deferred context (P3).
  ID3D11DeviceContext* active_context() const;
  ID3D11Buffer* active_mesh_cb() const;

  // D3D11 deferred-context parallel record (P3). No-op / fail when env off.
  long begin_deferred_draw(int worker_count);
  long bind_deferred_worker(int slot);  // slot < 0 clears TLS
  long finish_deferred_draw();
  bool deferred_draw_active() const;

  // Look up GPU texture resources by leftover Texture handle.
  ID3D11ShaderResourceView* texture_srv(uint handle) const;
  ID3D11SamplerState* linear_sampler();
  Texture* bound_texture() const { return bound_texture_; }

 private:
  template <typename T>
  static void safe_release(T*& ptr) {
    if (ptr) {
      ptr->Release();
      ptr = nullptr;
    }
  }

  // GPU backing for a leftover texture / renderbuffer handle.
  struct D3dGpuTexture {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
  };

  struct D3dGpuFbo {
    uint color_tex_handle = 0;
    uint depth_tex_handle = 0;
    RenderBuffer* depth_rb = nullptr;
  };

  struct D3dFontSlot {
    HFONT font = nullptr;
    int height_px = 16;
  };

  long create_swapchain_and_targets();
  long resize_targets(UINT width, UINT height, bool resize_buffers = true);
  void release_targets();
  void release_mesh_pipeline();
  void release_gpu_resources();
  long ensure_mesh_pipeline();
  // Upload MVP without Eigen product (debug Eigen transpose spills smash COM Map).
  long sync_mesh_constants(ID3D11DeviceContext* ctx, bool use_tex);
  Matrix& active_matrix();
  const Matrix& active_matrix() const;
  uint alloc_texture_handle();
  void release_gpu_texture(D3dGpuTexture& gpu);
  long draw_text_gdi(uint font_id, float xscreen, float yscreen,
                     const Color& color, const char* text);

  HWND hwnd_;
  UINT backbuffer_width_;
  UINT backbuffer_height_;

  ID3D11Device* device_;
  ID3D11DeviceContext* context_;
  IDXGISwapChain* swapchain_;
  ID3D11RenderTargetView* rtv_;
  ID3D11DepthStencilView* dsv_;
  ID3D11Texture2D* depth_tex_;
  // Offscreen color target — Clear/Draw land here; swapchain is blit-only.
  // Avoids DXGI_SWAP_EFFECT_DISCARD undefined-backbuffer capture races.
  ID3D11Texture2D* color_tex_ = nullptr;
  // CPU-read staging copy of color_tex_ (filled in SwapBuffers).
  ID3D11Texture2D* capture_tex_ = nullptr;

  // Minimal lit mesh pipeline for DEM Terrain (VF_XYZ|VF_NORMAL|VF_DIFFUSE).
  ID3D11VertexShader* mesh_vs_ = nullptr;
  ID3D11PixelShader* mesh_ps_ = nullptr;
  ID3D11InputLayout* mesh_il_ = nullptr;
  ID3D11Buffer* mesh_cb_ = nullptr;
  ID3D11RasterizerState* mesh_rs_ = nullptr;
  ID3D11DepthStencilState* mesh_dss_ = nullptr;
  ID3D11BlendState* mesh_bs_ = nullptr;
  bool mesh_pipeline_ok_ = false;
  // Sticky mesh PSO binds across consecutive DrawPrimitives (china ~1.7k lines).
  bool mesh_draw_state_bound_ = false;
  float last_mesh_mvp_[16] = {};
  bool last_mesh_use_tex_ = false;
  bool mesh_cb_valid_ = false;

  // Leftover GL fixed-function light state (mirrored for D3D mesh PS).
  // Light directions are stored in eye-space at SetLight time (GL glLight
  // transforms GL_POSITION by the current modelview once).
  struct StoredLight {
    bool enabled = false;
    float eye_dir[3] = {0.f, 0.f, 1.f};  // toward light, eye-space
    float diffuse[3] = {1.f, 1.f, 1.f};
    float ambient[3] = {0.f, 0.f, 0.f};
  };
  StoredLight lights_[8] = {};
  float scene_ambient_[3] = {1.f, 1.f, 1.f};

  std::unique_ptr<D3dGpuStateManager> state_manager_;
  std::unique_ptr<D3dDeviceCaps> device_caps_;

  float clear_color_[4];
  float clear_depth_;
  UINT clear_stencil_;

  Matrix modelview_;
  Matrix projection_;
  // Small fixed stacks on the heap — embedding 32×2 Matrix in the device
  // object (~4KB+) corrupted the CRT heap during Init.
  static constexpr int kMatrixStackMax = 16;
  std::unique_ptr<Matrix[]> modelview_stack_;
  std::unique_ptr<Matrix[]> projection_stack_;
  int modelview_sp_ = 0;
  int projection_sp_ = 0;

  std::map<uint, D3dGpuTexture> gpu_textures_;
  uint next_texture_handle_ = 1;
  Texture* bound_texture_ = nullptr;

  std::map<uint, D3dGpuFbo> gpu_fbos_;
  uint next_fbo_handle_ = 1;
  uint bound_fbo_handle_ = 0;

  // RenderBuffer::GetHandle historically stores height; key by pointer.
  std::unordered_map<RenderBuffer*, D3dGpuTexture> gpu_renderbuffers_;

  std::vector<D3dFontSlot> fonts_;
  ID3D11SamplerState* linear_sampler_ = nullptr;

  // P3: deferred context slots (CreateDeferredContext). Empty when unused.
  std::vector<D3dDeferredSlot> deferred_slots_;
  bool deferred_recording_ = false;
};

}  // namespace detail
}  // namespace scenic

#if !defined(SCENIC_RENDER_D3D_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_render_d3d_d.lib")
#else
#pragma comment(lib, "scenic_render_d3d.lib")
#endif
#endif

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_
