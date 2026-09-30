// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_

#include <map>
#include <unordered_map>
#include <vector>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/impl/d3d/caps/device_caps.h"
#include "legacy/render/rhi3d/impl/d3d/paint/states_manager.h"
#include "legacy/render/rhi3d/impl/d3d/prerequisites.h"
#include "legacy/render/rhi3d/public/device/render_device.h"

namespace render {

// Leftover Smt3DRenderDevice backed by D3D11 (not D3DX / D3D9).
// Layout: host/ (Init/Present), resource/ (VB·IB·texture·FBO·font),
// paint/ (draw/matrix/state). HWND present stays on D3D11 swapchain.
class LEGACY_RENDER_EXPORT SmtD3DRenderDevice : public Smt3DRenderDevice {
 public:
  SmtD3DRenderDevice();
  explicit SmtD3DRenderDevice(HINSTANCE hDLL);
  ~SmtD3DRenderDevice() override;

  long Init(HWND hWnd, const char* logname) override;
  long Release() override;
  long Destroy() override;

  SmtGPUStateManager* GetStateManager() override;
  Smt3DDeviceCaps* GetDeviceCaps() override;

  long BeginRender() override;
  long EndRender() override;

  long DrawPrimitives(PrimitiveType type, SmtVertexBuffer* pVB,
                      ulong baseVertex, ulong primitiveCount) override;
  long DrawIndexedPrimitives(PrimitiveType type, SmtVertexBuffer* pVB,
                             SmtIndexBuffer* pIB, ulong baseIndex,
                             ulong primitiveCount) override;

  long DrawText(uint unID, float xpos, float ypos, float zpos,
                const SmtColor& color, const char*, ...) override;
  long DrawText(uint nID, float xscreen, float yscreen, const SmtColor& color,
                const char*, ...) override;
  // Screen-space BGRA sprite (center in top-down window pixels).
  long DrawScreenBgra(float cx, float cy, int w, int h,
                      const unsigned char* bgra);

  long SwapBuffers() override;

  long SetClearColor(const SmtColor& clr) override;
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

  long GetFrustum(SmtFrustum& frustum) override;

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
  long SetShadeMode(RenderStateValue rsv, float, const SmtColor& clr) override;

  long SetLight(int index, SmtLight* pLight) override;
  long SetAmbientLight(const SmtColor& clr) override;
  long SetTexture(SmtTexture* pTex) override;
  long SetMaterial(SmtMaterial* pMat) override;
  long SetFog(FogMode mode, const SmtColor& color, float density, float start,
              float end) override;

  SmtVertexBuffer* CreateVertexBuffer(int nCount, ulong ulFormat,
                                      bool bDynamic = true) override;
  SmtIndexBuffer* CreateIndexBuffer(int nCount) override;

  SmtVideoBuffer* CreateVideoBuffer(ArrayType type) override;
  long DestroyBuffer(SmtVideoBuffer* buffer) override;
  long DestroyIndexBuffer(SmtVideoBuffer* buffer) override;

  long SetVertexArray(int components, Type type, int stride,
                      void* data) override;
  long SetTextureCoordsArray(int components, Type type, int stride,
                             void* data) override;
  long SetNormalArray(Type type, int stride, void* data) override;
  long SetIndexArray(Type type, int stride, void* data) override;
  long EnableArray(ArrayType type, bool enabled) override;

  long BindBuffer(SmtVideoBuffer* buffer) override;
  long BindIndexBuffer(SmtVideoBuffer* buffer) override;
  long UnbindBuffer() override;
  long UnbindIndexBuffer() override;
  long UpdateBuffer(SmtVideoBuffer* buffer, void* data, uint size,
                    VideoBufferStoreMethod method) override;
  long UpdateIndexBuffer(SmtVideoBuffer* buffer, void* data, uint size,
                         VideoBufferStoreMethod method) override;
  void* MapBuffer(SmtVideoBuffer* buffer, AccessMode access) override;
  long UnmapBuffer(SmtVideoBuffer* buffer) override;
  void* MapIndexBuffer(SmtVideoBuffer* buffer, AccessMode access) override;
  long UnmapIndexBuffer(SmtVideoBuffer* buffer) override;

  SmtShader* CreateVertexShader(const char* szName) override;
  SmtShader* CreatePixelShader(const char* szName) override;
  long DestroyShader(const char* szName) override;
  SmtShader* GetShader(const char* szName) override;

  SmtProgram* CreateProgram(const char* szName) override;
  long DestroyProgram(const char* szName) override;
  SmtProgram* GetProgram(const char* szName) override;

  long LoadShaderSource(SmtShader* shader, char* source) override;
  long CompileShader(SmtShader* shader) override;
  long IsShaderCompiled(SmtShader* shader) override;
  char* GetShaderLog(SmtShader* shader) override;

  long BindProgram(SmtProgram* program) override;
  long UnbindProgram() override;
  long SetProgramVertexShader(SmtProgram* program, SmtShader* shader) override;
  long SetProgramPixelShader(SmtProgram* program, SmtShader* shader) override;
  long LinkProgram(SmtProgram* program) override;
  long IsProgramLinked(SmtProgram* program) override;
  char* GetProgramLinkLog(SmtProgram* program) override;

  long SetProgramVector(SmtProgram* program, string& param,
                        const Vector4& value) override;
  long SetProgramVector(SmtProgram* program, string& param,
                        const Vector3& value) override;
  long SetProgramVector(SmtProgram* program, string& param,
                        const Vector2& value) override;
  long SetProgramFloat(SmtProgram* program, string& param,
                       float value) override;
  long SetProgramInt(SmtProgram* program, string& param, int value) override;
  long GetProgramFloat(SmtProgram* program, string& param,
                       float* value) override;
  long SetProgramTexture(SmtProgram* program, string& param,
                         int texture) override;

  SmtTexture* CreateTexture(const char* szName) override;
  long DestroyTexture(const char* szName) override;
  SmtTexture* GetTexture(const char* szName) override;

  long GenerateMipmap(SmtTexture* texture) override;
  long BindTexture(SmtTexture* texture) override;
  long BuildTexture(SmtTexture* texture) override;
  long BindRectTexture(SmtTexture* texture) override;
  long UnbindTexture() override;
  long UnbindRectTexture(void) override;

  SmtFrameBuffer* CreateFrameBuffer() override;
  long DestroyFrameBuffer(SmtFrameBuffer* frameBuffer) override;
  long BindFrameBuffer(SmtFrameBuffer* frameBuffer) override;
  long UnbindFrameBuffer() override;
  SmtRenderBuffer* CreateRenderBuffer(TextureFormat format, uint width,
                                      uint height) override;
  long DestroyRenderBuffer(SmtRenderBuffer* renderBuffer) override;
  long AttachRenderBuffer(SmtFrameBuffer* frameBuffer,
                          SmtRenderBuffer* renderBuffer,
                          RenderBufferSlot slot) override;
  long AttachTexture(SmtFrameBuffer* frameBuffer, SmtTexture* texture2D,
                     RenderBufferSlot slot) override;
  FrameBufferStatus CheckFrameBufferStatus() override;

  long CreateFont(const char* szChType, int nHeight, int nWidth, int nWeight,
                  bool bItalic, bool bUnderline, bool bStrike, ulong dwSize,
                  uint& unID) override;

  long DrawCube3D(Vector3 vCenter, float fWidth, SmtColor smtClr) override;

  // Read presented backbuffer as tightly packed BGR24 (bottom-up, BMP order).
  long CaptureBgr24(unsigned char* out_bgr24, int width_px, int height_px);

  ID3D11Device* device() const { return device_; }
  ID3D11DeviceContext* context() const { return context_; }

  // Look up GPU texture resources by leftover SmtTexture handle.
  ID3D11ShaderResourceView* texture_srv(uint handle) const;
  ID3D11SamplerState* linear_sampler();
  SmtTexture* bound_texture() const { return bound_texture_; }

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
    SmtRenderBuffer* depth_rb = nullptr;
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
  Matrix& active_matrix();
  const Matrix& active_matrix() const;
  uint alloc_texture_handle();
  void release_gpu_texture(D3dGpuTexture& gpu);
  long draw_text_gdi(uint font_id, float xscreen, float yscreen,
                     const SmtColor& color, const char* text);

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

  // Minimal lit mesh pipeline for StereoTerrain (VF_XYZ|VF_NORMAL|VF_DIFFUSE).
  ID3D11VertexShader* mesh_vs_ = nullptr;
  ID3D11PixelShader* mesh_ps_ = nullptr;
  ID3D11InputLayout* mesh_il_ = nullptr;
  ID3D11Buffer* mesh_cb_ = nullptr;
  ID3D11RasterizerState* mesh_rs_ = nullptr;
  ID3D11DepthStencilState* mesh_dss_ = nullptr;
  ID3D11BlendState* mesh_bs_ = nullptr;
  bool mesh_pipeline_ok_ = false;

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

  SmtD3DGPUStateManager* state_manager_;
  SmtD3DDeviceCaps* device_caps_;

  float clear_color_[4];
  float clear_depth_;
  UINT clear_stencil_;

  Matrix modelview_;
  Matrix projection_;
  // Small fixed stacks on the heap — embedding 32×2 Matrix in the device
  // object (~4KB+) corrupted the CRT heap before leftover_session init.
  static constexpr int kMatrixStackMax = 16;
  Matrix* modelview_stack_ = nullptr;
  Matrix* projection_stack_ = nullptr;
  int modelview_sp_ = 0;
  int projection_sp_ = 0;

  std::map<uint, D3dGpuTexture> gpu_textures_;
  uint next_texture_handle_ = 1;
  SmtTexture* bound_texture_ = nullptr;

  std::map<uint, D3dGpuFbo> gpu_fbos_;
  uint next_fbo_handle_ = 1;
  uint bound_fbo_handle_ = 0;

  // SmtRenderBuffer::GetHandle historically stores height; key by pointer.
  std::unordered_map<SmtRenderBuffer*, D3dGpuTexture> gpu_renderbuffers_;

  std::vector<D3dFontSlot> fonts_;
  ID3D11SamplerState* linear_sampler_ = nullptr;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_DEVICE_3DRENDERDEVICE_H_
