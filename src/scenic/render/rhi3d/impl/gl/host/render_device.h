// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GL_3DRENDERDEVICE_H
#define _GL_3DRENDERDEVICE_H

#include <string>
#include <string_view>

#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/impl/gl/paint/states_manager.h"
#include "scenic/render/rhi3d/impl/gl/ext/fbo_func.h"
#include "scenic/render/rhi3d/impl/gl/ext/mipmap_func.h"
#include "scenic/render/rhi3d/impl/gl/ext/multitexture_func.h"
#include "scenic/render/rhi3d/impl/gl/ext/shader_func.h"
#include "scenic/render/rhi3d/impl/gl/ext/vbo_func.h"
#include "scenic/render/rhi3d/impl/gl/ext/vsync_func.h"
#include "scenic/render/rhi3d/impl/gl/prerequisites.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
namespace scenic {
namespace detail {
#define MAX_LIGHTS 8

class GlText;
class GlDeviceCaps;

class LEGACY_RENDER_GL_EXPORT GlRenderDevice : public RenderDevice3d {
 public:
  GlRenderDevice(void);
  GlRenderDevice(HINSTANCE hDLL);
  virtual ~GlRenderDevice(void);

 public:
  virtual long Init(HWND hWnd, const char *logname);  // initial
  virtual long Release(void);  // release ptr or close file
  virtual long Destroy(void);  // destroy gl object

  inline RenderBase3DApi GetBaseApi() const { return m_rBaseApi; }

  // Token match against GL_EXTENSIONS (space-separated). Cached after first
  // query while a context is current.
  bool IsExtensionSupported(std::string_view extension);

  void* GetProcAddress(std::string_view name) {
    // wglGetProcAddress requires a NUL-terminated name.
    const std::string zname(name);
    return wglGetProcAddress(zname.c_str());
  }

  virtual GpuStateManager *GetStateManager() { return m_pStateManager; }
  // Defined after GlDeviceCaps is complete (see device_caps.h include cycle).
  virtual DeviceCaps3d *GetDeviceCaps();

 public:
  // render
  virtual long BeginRender();
  virtual long EndRender();

  virtual long DrawPrimitives(PrimitiveType type, VertexBuffer *pVB,
                              ulong baseVertex, ulong primitiveCount);
  virtual long DrawIndexedPrimitives(PrimitiveType type, VertexBuffer *pVB,
                                     IndexBuffer *pIB, ulong baseIndex,
                                     ulong primitiveCount);

  virtual long DrawText(uint unID, float xpos, float ypos, float zpos,
                        const Color &color, const char *, ...);
  virtual long DrawText(uint nID, float xscreen, float yscreen,
                        const Color &color, const char *, ...);

  virtual long SwapBuffers();

  virtual long SetClearColor(const Color &clr);
  virtual long SetDepthClearValue(float z);
  virtual long SetStencilClearValue(ulong s);
  virtual long Clear(ulong flags);

  // matrix
  virtual long MatrixModeSet(MatrixMode mode);
  virtual MatrixMode MatrixModeGet() const;
  virtual long MatrixLoadIdentity();

  virtual long MatrixLoad(const Matrix &m);
  virtual long MatrixPush();
  virtual long MatrixPop();
  virtual long MatrixScale(float x, float y, float z);
  virtual long MatrixTranslation(float x, float y, float z);
  virtual long MatrixRotation(float angle, float x, float y, float z);
  virtual long MatrixMultiply(const Matrix &m);
  virtual Matrix MatrixGet();

  // get frustum
  virtual long GetFrustum(Frustum& frustum);

  // view
  virtual long SetViewport(Viewport3D &viewport);
  virtual Viewport3D &GetViewport(void) { return m_viewPort; }

  virtual long SetOrtho(float left, float right, float bottom, float top,
                        float zNear, float zFar);
  virtual long SetPerspective(float fovy, float aspect, float zNear,
                              float zFar);

  // set viewlookat
  virtual long SetViewLookAt(Vector3 &vPos, Vector3 &vView, Vector3 &vUp);

  // 2d to 3d
  virtual long Transform2DTo3D(Vector3 &vOrg, Vector3 &vTar,
                               const lPoint &point);

  // 3d to 2d
  virtual long Transform3DTo2D(const Vector3 &ver3D, lPoint &point);

 public:
  // aactivate additive blending
  virtual long SetBlending(bool bBlending);

  // activate backface culling
  virtual long SetBackfaceCulling(RenderStateValue rsv);

  // activate stencil buffer
  virtual long SetStencilBufferMode(RenderStateValue rsv, ulong ul);

  // activate depth buffer
  virtual long SetDepthBufferMode(RenderStateValue rsv);

  // activate wireframe mode
  virtual long SetShadeMode(RenderStateValue rsv, float, const Color &clr);

 public:
  // light stuff
  virtual long SetLight(int index, Light *pLight);
  virtual long SetAmbientLight(const Color &clr);

  // set texture
  virtual long SetTexture(Texture *pTex);

  // material stuff
  virtual long SetMaterial(Material *pMat);

  // fog stuff
  virtual long SetFog(FogMode mode, const Color &color, float density,
                      float start, float end);

 public:
  // vertex buffer
  virtual VertexBuffer *CreateVertexBuffer(int nCount, ulong ulFormat,
                                              bool bDynamic = true);

  // index buffer
  virtual IndexBuffer *CreateIndexBuffer(int nCount);

  // video buffer
  virtual VideoBuffer *CreateVideoBuffer(ArrayType type);
  virtual long DestroyBuffer(VideoBuffer *buffer);
  virtual long DestroyIndexBuffer(VideoBuffer *buffer);

  // vba
  virtual long SetVertexArray(int components, Type type, int stride,
                              void *data);
  virtual long SetTextureCoordsArray(int components, Type type, int stride,
                                     void *data);
  virtual long SetNormalArray(Type type, int stride, void *data);
  virtual long SetIndexArray(Type type, int stride, void *data);
  virtual long EnableArray(ArrayType type, bool enabled);

  // vbo
  virtual long BindBuffer(VideoBuffer *buffer);
  virtual long BindIndexBuffer(VideoBuffer *buffer);
  virtual long UnbindBuffer();
  virtual long UnbindIndexBuffer();
  virtual long UpdateBuffer(VideoBuffer *buffer, void *data, uint size,
                            VideoBufferStoreMethod method);
  virtual long UpdateIndexBuffer(VideoBuffer *buffer, void *data, uint size,
                                 VideoBufferStoreMethod method);
  virtual void *MapBuffer(VideoBuffer *buffer, AccessMode access);
  virtual long UnmapBuffer(VideoBuffer *buffer);
  virtual void *MapIndexBuffer(VideoBuffer *buffer, AccessMode access);
  virtual long UnmapIndexBuffer(VideoBuffer *buffer);

  // shader
  virtual Shader *CreateVertexShader(const char *szName);  // vertex shader
  virtual Shader *CreatePixelShader(const char *szName);   // pixel shader
  virtual long DestroyShader(const char *szName);
  virtual Shader *GetShader(const char *szName);

  // program
  virtual Program *CreateProgram(const char *szName);
  virtual long DestroyProgram(const char *szName);
  virtual Program *GetProgram(const char *szName);

  virtual long LoadShaderSource(Shader *shader, char *source);
  virtual long CompileShader(Shader *shader);
  virtual long IsShaderCompiled(Shader *shader);
  virtual char *GetShaderLog(Shader *shader);

  virtual long BindProgram(Program *program);
  virtual long UnbindProgram();
  virtual long SetProgramVertexShader(Program *program, Shader *shader);
  virtual long SetProgramPixelShader(Program *program, Shader *shader);
  virtual long LinkProgram(Program *program);
  virtual long IsProgramLinked(Program *program);
  virtual char *GetProgramLinkLog(Program *program);

  virtual long SetProgramVector(Program *program, string &param,
                                const Vector4 &value);
  virtual long SetProgramVector(Program *program, string &param,
                                const Vector3 &value);
  virtual long SetProgramVector(Program *program, string &param,
                                const Vector2 &value);
  virtual long SetProgramFloat(Program *program, string &param, float value);
  virtual long SetProgramInt(Program *program, string &param, int value);
  virtual long GetProgramFloat(Program *program, string &param,
                               float *value);

  virtual long SetProgramTexture(Program *program, string &param,
                                 int texture);

  // texture
  virtual Texture *CreateTexture(const char *szName);
  virtual long DestroyTexture(const char *szName);
  virtual Texture *GetTexture(const char *szName);

  virtual long GenerateMipmap(Texture *texture);
  virtual long BindTexture(Texture *texture);
  virtual long BuildTexture(Texture *texture);
  virtual long BindRectTexture(Texture *texture);
  virtual long UnbindTexture();
  virtual long UnbindRectTexture(void);

  virtual FrameBuffer *CreateFrameBuffer();
  virtual long DestroyFrameBuffer(FrameBuffer *frameBuffer);

  virtual long BindFrameBuffer(FrameBuffer *frameBuffer);
  virtual long UnbindFrameBuffer();
  virtual RenderBuffer *CreateRenderBuffer(TextureFormat format, uint width,
                                              uint height);
  virtual long DestroyRenderBuffer(RenderBuffer *renderBuffer);

  virtual long AttachRenderBuffer(FrameBuffer *frameBuffer,
                                  RenderBuffer *renderBuffer,
                                  RenderBufferSlot slot);
  virtual long AttachTexture(FrameBuffer *frameBuffer, Texture *texture2D,
                             RenderBufferSlot slot);
  virtual FrameBufferStatus CheckFrameBufferStatus();

  // ceate font
  virtual long CreateFont(const char *szChType, int nHeight, int nWidth,
                          int nWeight, bool bItalic, bool bUnderline,
                          bool bStrike, ulong dwSize, uint &unID);

 public:
  // fast draw
  virtual long DrawCube3D(Vector3 vCenter, float fWidth, Color smtClr);

 protected:
  long SetDeviceCaps(void);
  inline long GetOpenGLPrimitiveType(const PrimitiveType pt,
                                     const ulong nInitialPrimitiveCount,
                                     GLenum *GLPrimitiveType,
                                     ulong *nGLPrimitiveCount);

  GLenum ConvertRenderBufferSlot(RenderBufferSlot slot);
  GLenum ConvertType(Type type);
  GLenum ConvertVideoBufferStoreMethod(VideoBufferStoreMethod method);
  GLenum ConvertAccess(AccessMode access);
  GLenum ConvertArrayType(ArrayType type);
  GLenum ConvertTexFormat(TextureFormat format);

 protected:
  vector<GLhandleARB> m_vGLHandles;

  GlGpuStateManager *m_pStateManager;
  GlDeviceCaps *m_pDeviceCaps;

  // ext func
  ShadersFunc *m_pFuncShaders;
  MultitextureFunc *m_pFuncMultTex;
  VSyncFunc *m_pFuncVSync;
  MipmapFunc *m_pFuncMipmap;
  VboFunc *m_pFuncVBO;
  FboFunc *m_pFuncFBO;

 protected:
  HWND m_hWnd;
  HDC m_hPaintDC;
  HGLRC m_hRC;
  vector<GlText *> m_vTextPtrs;

 private:
  int m_nStencilRef = 0;
  uint m_unStencilCmp = 0;         // stencil cmp function
  uint m_unStencilWriteMask = 0;   // WriteMask
  uint m_unStencilMask = 0;        // mask
  uint m_unOpStencilFail = 0;      // stencil fail op
  uint m_unOpStencilZPass = 0;     // stencil pass
  uint m_unOpStencilZFail = 0;     // stencil pass but zbuf fail op
  std::string gl_extensions_cache_;
};
}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_GL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_render_gl_d.lib")
#else
#pragma comment(lib, "scenic_render_gl.lib")
#endif
#endif

#endif  //_GL_3DRENDERDEVICE_H
