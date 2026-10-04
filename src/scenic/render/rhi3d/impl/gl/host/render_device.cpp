// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

#include <cstring>
#include <memory>

#include "base/core/log.h"
#include "scenic/render/rhi2d/public/device/render_device.h"
#include "scenic/render/rhi3d/impl/gl/caps/device_caps.h"
#include "scenic/render/rhi3d/impl/gl/ext/fbo_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/ext/mipmap_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/ext/multitexture_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/ext/shader_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/ext/vbo_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/ext/vsync_func_imp.h"
#include "scenic/render/rhi3d/impl/gl/resource/text/text.h"
#include "scenic/render/rhi3d/public/shader/program_manager.h"
#include "scenic/render/rhi3d/public/shader/shader_manager.h"
#include "scenic/render/rhi3d/public/texture/texture_manager.h"

using namespace base;

namespace scenic {
namespace detail {
DeviceCaps3d* GlRenderDevice::GetDeviceCaps() {
  return static_cast<DeviceCaps3d*>(m_pDeviceCaps.get());
}

GlRenderDevice::GlRenderDevice()
    : m_hWnd(nullptr), m_hPaintDC(nullptr), m_hRC(nullptr) {
  m_rBaseApi = RA_OPENGL;
  m_hDLL = nullptr;
  m_strLogName.clear();
  m_pStateManager = std::make_unique<GlGpuStateManager>();
}

GlRenderDevice::GlRenderDevice(HINSTANCE hDLL)
    : m_hWnd(nullptr), m_hPaintDC(nullptr), m_hRC(nullptr) {
  m_rBaseApi = RA_OPENGL;
  m_hDLL = hDLL;
  m_strLogName.clear();
  m_pStateManager = std::make_unique<GlGpuStateManager>();
}

bool GlRenderDevice::IsExtensionSupported(std::string_view extension) {
  if (extension.empty()) {
    return false;
  }
  if (gl_extensions_cache_.empty()) {
    const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!ext) {
      return false;
    }
    gl_extensions_cache_ = ext;
  }
  // Space-delimited token match (avoid substring false positives).
  const char* hay = gl_extensions_cache_.c_str();
  const size_t needle_len = extension.size();
  while (*hay) {
    while (*hay == ' ') {
      ++hay;
    }
    if (!*hay) {
      break;
    }
    const char* end = hay;
    while (*end && *end != ' ') {
      ++end;
    }
    const size_t token_len = static_cast<size_t>(end - hay);
    if (token_len == needle_len &&
        std::strncmp(hay, extension.data(), needle_len) == 0) {
      return true;
    }
    hay = end;
  }
  return false;
}

GlRenderDevice::~GlRenderDevice() { Release(); }

long GlRenderDevice::Init(HWND hWnd, const char *logname) {
  assert(::IsWindow(hWnd));
  m_hWnd = hWnd;

  LOGGING(LOG_INFO, "Init OpenGL 3DRenderDevice ok!");

  m_strLogName = logname;

  PIXELFORMATDESCRIPTOR pfd;
  memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.iLayerType = PFD_MAIN_PLANE;

  HDC hDC = ::GetDC(m_hWnd);

  // Choose pixel format
  int nPixelFormat = ChoosePixelFormat(hDC, &pfd);
  if (nPixelFormat == 0) {
    return kErrFailure;
  } else {
    // Set pixel format
    BOOL bResult = SetPixelFormat(hDC, nPixelFormat, &pfd);
    if (!bResult) {
      return kErrFailure;
    } else {
      // Create a rendering context.
      m_hRC = wglCreateContext(hDC);
      if (!m_hRC) {
        return kErrFailure;
      } else {
        // Set it as the current context
        if (!wglMakeCurrent(hDC, m_hRC)) {
          return kErrFailure;
        }
      }
    }
  }

  // Keep this HDC for the RC lifetime. ReleaseDC here used to leave
  // later gl*/wglUseFont* on a stale DC (hang or STATUS_FATAL_APP_EXIT).
  m_hPaintDC = hDC;

  if (kErrNone != SetDeviceCaps()) return kErrFailure;

  // Set a default viewport
  glEnable(GL_SCISSOR_TEST);
  glDepthFunc(GL_LESS);
  glDisable(GL_TEXTURE_2D);

  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  RECT wndbfRect;
  Viewport3D viewport;
  GetClientRect(m_hWnd, &wndbfRect);

  viewport.ulHeight = wndbfRect.bottom - wndbfRect.top;
  viewport.ulWidth = wndbfRect.right - wndbfRect.left;
  viewport.ulX = 0;
  viewport.ulY = 0;
  viewport.fZNear = 0.1f;
  viewport.fZFar = 1000.f;
  viewport.fFovy = 45.f;
  SetViewport(viewport);

  SetClearColor(Color(1, 0, 0));
  SetDepthClearValue(1.0f);
  SetStencilClearValue(0);

  return kErrNone;
}

long GlRenderDevice::Destroy() {
  gl_extensions_cache_.clear();
  // ~GlText calls glDeleteLists. That AV's if it runs after
  // wglDeleteContext, which is what window close used to do: Destroy()
  // dropped the context, then ~GlRenderDevice::Release() freed fonts.
  const int nFont = static_cast<int>(m_vTextPtrs.size());
  for (int i = 0; i < nFont; i++) {
    // ~GlText issues glDeleteLists; run while the context is still current.
    m_vTextPtrs[i].reset();
  }
  m_vTextPtrs.clear();

  vector<string> vStrResNames;
  m_textureMgr.GetAllTextureName(vStrResNames);
  for (int i = 0; i < vStrResNames.size(); i++) {
    Texture *pTexture = m_textureMgr.GetTexture(vStrResNames[i].c_str());
    if (pTexture) {
      GLhandleARB handle = pTexture->GetHandle();
      if (handle != 0) glDeleteTextures(1, &handle);
    }
  }
  m_textureMgr.DestroyAllTexture();

  m_shaderMgr.GetAllShaderName(vStrResNames);
  for (int i = 0; i < vStrResNames.size(); i++) {
    Shader *pShader = m_shaderMgr.GetShader(vStrResNames[i].c_str());
    if (pShader) {
      GLhandleARB handle = pShader->GetHandle();
      if (handle != 0) m_pFuncShaders->glDeleteObject(handle);
    }
  }
  m_shaderMgr.DestroyAllShader();

  m_progamMgr.GetAllProgramName(vStrResNames);
  for (int i = 0; i < vStrResNames.size(); i++) {
    Program *pProgram = m_progamMgr.GetProgram(vStrResNames[i].c_str());
    if (pProgram) {
      GLhandleARB handle = pProgram->GetHandle();
      if (handle != 0) m_pFuncShaders->glDeleteObject(handle);
    }
  }
  m_progamMgr.DestroyAllProgram();

  if (::wglGetCurrentContext()) ::wglMakeCurrent(nullptr, nullptr);

  if (m_hPaintDC && m_hWnd) {
    ::ReleaseDC(m_hWnd, m_hPaintDC);
    m_hPaintDC = nullptr;
  }

  if (m_hRC) {
    ::wglDeleteContext(m_hRC);
    m_hRC = nullptr;
  }

  m_pDeviceCaps.reset();
  m_pFuncShaders.reset();
  m_pFuncMultTex.reset();
  m_pFuncVSync.reset();
  m_pFuncMipmap.reset();
  m_pFuncVBO.reset();
  m_pFuncFBO.reset();

  return kErrNone;
}

long GlRenderDevice::Release() {
  m_pStateManager.reset();
  m_vTextPtrs.clear();
  return kErrNone;
}

long GlRenderDevice::SetDeviceCaps(void) {
  m_pDeviceCaps = std::make_unique<GlDeviceCaps>(this);

  /* Init ARB_multitexture */
  if (m_pDeviceCaps->IsMultiTextureSupported()) {
    m_pFuncMultTex = std::make_unique<MultitextureFuncImpl>();
  } else {
    m_pFuncMultTex = std::make_unique<MultitextureFunc>();
  }

  if (!m_pFuncMultTex || kErrNone != m_pFuncMultTex->Initialize(this)) {
    return kErrFailure;
  }

  /* Init GL_ARB_vertex_buffer_object */
  if (m_pDeviceCaps->IsVBOSupported()) {
    m_pFuncVBO = std::make_unique<VboFuncImpl>();
  } else {
    m_pFuncVBO = std::make_unique<VboFunc>();
  }

  if (!m_pFuncVBO || kErrNone != m_pFuncVBO->Initialize(this)) {
    return kErrFailure;
  }

  /* Init shaders */
  if (m_pDeviceCaps->IsGLSLSupported()) {
    m_pFuncShaders = std::make_unique<ShadersFuncImpl>();
  } else {
    m_pFuncShaders = std::make_unique<ShadersFunc>();
  }

  if (!m_pFuncShaders || kErrNone != m_pFuncShaders->Initialize(this)) {
    return kErrFailure;
  }

  /* Init frame buffer objects */
  if (m_pDeviceCaps->IsFBOSupported()) {
    m_pFuncFBO = std::make_unique<FboFuncImpl>();
  } else {
    m_pFuncFBO = std::make_unique<FboFunc>();
  }

  if (!m_pFuncFBO || kErrNone != m_pFuncFBO->Initialize(this)) {
    return kErrFailure;
  }

  /* Init mimmap generation */
  if (m_pDeviceCaps->IsMipMapsSupported()) {
    m_pFuncMipmap = std::make_unique<MipmapFuncImpl>();
  } else {
    m_pFuncMipmap = std::make_unique<MipmapFunc>();
  }

  if (!m_pFuncMipmap || kErrNone != m_pFuncMipmap->Initialize(this)) {
    return kErrFailure;
  }

  /* Init VSync extension */
  if (m_pDeviceCaps->IsVSyncSupported()) {
    m_pFuncVSync = std::make_unique<VSyncFuncImpl>();
  } else {
    m_pFuncVSync = std::make_unique<VSyncFunc>();
  }

  if (!m_pFuncVSync || kErrNone != m_pFuncVSync->Initialize(this)) {
    return kErrFailure;
  }

  return kErrNone;
}

GLenum GlRenderDevice::ConvertType(Type type) {
  switch (type) {
    case kShort:
      return GL_SHORT;
    case kInt:
      return GL_INT;
    case kFloat:
      return GL_FLOAT;
    case kDouble:
      return GL_DOUBLE;
    case kUnsignedInt:
      return GL_UNSIGNED_INT;
    case kUnsignedByte:
      return GL_UNSIGNED_BYTE;
    case kUnsignedShort:
      return GL_UNSIGNED_SHORT;
  }
  return -1;
}

GLenum GlRenderDevice::ConvertVideoBufferStoreMethod(
    VideoBufferStoreMethod method) {
  switch (method) {
    case STATIC_DRAW:
      return GL_STATIC_DRAW_ARB;
    case STATIC_READ:
      return GL_STATIC_READ_ARB;
    case STATIC_COPY:
      return GL_STATIC_COPY_ARB;
    case DYNAMIC_DRAW:
      return GL_DYNAMIC_DRAW_ARB;
    case DYNAMIC_READ:
      return GL_DYNAMIC_READ_ARB;
    case DYNAMIC_COPY:
      return GL_DYNAMIC_COPY_ARB;
    case STREAM_DRAW:
      return GL_STREAM_DRAW_ARB;
    case STREAM_READ:
      return GL_STREAM_READ_ARB;
    case STREAM_COPY:
      return GL_STREAM_COPY_ARB;
  }

  return -1;
}

GLenum GlRenderDevice::ConvertAccess(AccessMode access) {
  switch (access) {
    case READ_ONLY:
      return GL_READ_ONLY_ARB;
    case WRITE_ONLY:
      return GL_WRITE_ONLY_ARB;
    case READ_WRITE:
      return GL_READ_WRITE_ARB;
  }
  return -1;
}

GLenum GlRenderDevice::ConvertArrayType(ArrayType type) {
  switch (type) {
    case TEXTURE_COORD_ARRAY:
      return GL_TEXTURE_COORD_ARRAY;
    case COLOR_ARRAY:
      return GL_COLOR_ARRAY;
    case INDEX_ARRAY:
      return GL_INDEX_ARRAY;
    case NORMAL_ARRAY:
      return GL_NORMAL_ARRAY;
    case VERTEX_ARRAY:
      return GL_VERTEX_ARRAY;
  }
  return -1;
}

GLenum GlRenderDevice::ConvertTexFormat(TextureFormat format) {
  switch (format) {
    case RGB8:
      return GL_RGB;
    case RGBA8:
      return GL_RGBA;
    case RGB_DXT1:
      return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case RGBA_DXT1:
      return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case RGBA_DXT3:
      return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case RGBA_DXT5:
      return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case LUMINANCE8:
      return GL_LUMINANCE;
    case INTENSITY8:
      return GL_INTENSITY;
    case RGB16F:
      return GL_RGB16F_ARB;
    case RGBA16F:
      return GL_RGBA16F_ARB;
    case ALPHA16F:
      return GL_ALPHA16F_ARB;
    case INTENSITY16F:
      return GL_INTENSITY16F_ARB;
    case LUMINANCE16F:
      return GL_LUMINANCE16F_ARB;
    case LUMINANCE_ALPHA16F:
      return GL_LUMINANCE_ALPHA16F_ARB;
    case RGB32F:
      return GL_RGB32F_ARB;
    case RGBA32F:
      return GL_RGBA32F_ARB;
    case ALPHA32F:
      return GL_ALPHA32F_ARB;
    case INTENSITY32F:
      return GL_INTENSITY32F_ARB;
    case LUMINANCE32F:
      return GL_LUMINANCE32F_ARB;
    case LUMINANCE_ALPHA32F:
      return GL_LUMINANCE_ALPHA32F_ARB;
  }

  return (GLenum)ERROR;
}

}  // namespace detail
}  // namespace scenic