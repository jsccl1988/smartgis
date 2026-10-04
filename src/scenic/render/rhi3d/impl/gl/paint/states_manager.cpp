#include "scenic/render/rhi3d/impl/gl/paint/states_manager.h"

namespace scenic {
namespace detail {
/**
Map to convert OpenGL consts to engine's and vice versa
*/
const static int g_GLToEngineMap[][2] = {
    {GL_LESS, CMP_LESS},
    {GL_LEQUAL, CMP_LEQUAL},
    {GL_GREATER, CMP_GREATER},
    {GL_GEQUAL, CMP_GEQUAL},
    {GL_EQUAL, CMP_EQUAL},
    {GL_ZERO, BF_ZERO},
    {GL_ONE, BF_ONE},
    {GL_SRC_COLOR, BF_SRC_COLOR},
    {GL_DST_COLOR, BF_DST_COLOR},
    {GL_ONE_MINUS_DST_COLOR, BF_ONE_MINUS_DST_COLOR},
    {GL_SRC_ALPHA, BF_SRC_ALPHA},
    {GL_ONE_MINUS_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA},
    {GL_DST_ALPHA, BF_DST_ALPHA},
    {GL_ONE_MINUS_DST_ALPHA, BF_ONE_MINUS_DST_ALPHA},
    {GL_SRC_ALPHA_SATURATE, BF_SRC_ALPHA_SATURATE}};

/* Indexes of map tables for different constants */
const static int COMPARISON_TABLE = 0;
const static int TEXTURE_FILTER_TABLE = 1;
const static int TEXTURE_COORD_WRAP_MODE_TABLE = 2;
const static int TEXTURE_ENVIRONMENT_TABLE = 3;
const static int FACE_TABLE = 4;
const static int POLYGON_MODE_TABLE = 5;
const static int BLEND_FACTOR_TABLE = 6;

const static int g_ComparisonTable[] = {GL_LESS, GL_LEQUAL, GL_GREATER,
                                        GL_GEQUAL, GL_EQUAL};

const static int g_TextureFilterTable[] = {GL_NEAREST,
                                           GL_LINEAR,
                                           GL_NEAREST_MIPMAP_NEAREST,
                                           GL_LINEAR_MIPMAP_NEAREST,
                                           GL_NEAREST_MIPMAP_LINEAR,
                                           GL_LINEAR_MIPMAP_LINEAR};

const static int g_TextureCoordWrapModeTable[] = {GL_CLAMP, GL_REPEAT,
                                                  GL_CLAMP_TO_EDGE};

const static int g_TextureEnvironmentTable[] = {GL_REPLACE, GL_MODULATE,
                                                GL_DECAL, GL_BLEND, GL_ADD};

const static int g_FaceTable[] = {GL_BACK, GL_FRONT, GL_FRONT_AND_BACK};

const static int g_PolygonModeTable[] = {GL_POINT, GL_LINE, GL_FILL};

const static int g_BlendFactorTable[] = {GL_ZERO,
                                         GL_ONE,
                                         GL_SRC_COLOR,
                                         GL_DST_COLOR,
                                         GL_ONE_MINUS_DST_COLOR,
                                         GL_SRC_ALPHA,
                                         GL_ONE_MINUS_SRC_ALPHA,
                                         GL_DST_ALPHA,
                                         GL_ONE_MINUS_DST_ALPHA,
                                         GL_SRC_ALPHA_SATURATE};

const static int *g_EngineToGLMap[] = {g_ComparisonTable,
                                       g_TextureFilterTable,
                                       g_TextureCoordWrapModeTable,
                                       g_TextureEnvironmentTable,
                                       g_FaceTable,
                                       g_PolygonModeTable,
                                       g_BlendFactorTable};

const static int g_TableSize[] = {
    sizeof(g_ComparisonTable) / sizeof(int),
    sizeof(g_TextureFilterTable) / sizeof(int),
    sizeof(g_TextureCoordWrapModeTable) / sizeof(int),
    sizeof(g_TextureEnvironmentTable) / sizeof(int),
    sizeof(g_FaceTable) / sizeof(int),
    sizeof(g_PolygonModeTable) / sizeof(int),
    sizeof(g_BlendFactorTable) / sizeof(int)};

const static int g_TablesCount = sizeof(g_EngineToGLMap) / sizeof(int *);

/**
Default constructor.
*/
GlGpuStateManager::GlGpuStateManager() {
  for (int i = 0; i < g_TablesCount; i++) {
    m_GLToDev[g_GLToEngineMap[i][0]] = g_GLToEngineMap[i][1];
  }
}

int GlGpuStateManager::ConvertGLEnum(GLenum value) {
  return m_GLToDev[(int)value];
}

GLenum GlGpuStateManager::ConvertToGLEnum(uint tableIndex, uint value) {
  if (tableIndex < g_TablesCount && value < g_TableSize[tableIndex]) {
    return g_EngineToGLMap[tableIndex][value];
  }

  return -1;
}

AlphaTestState GlGpuStateManager::GetAlphaTestState() {
  AlphaTestState newState;
  int func = 0;
  float ref = 0;

  glGetIntegerv(GL_ALPHA_TEST_FUNC, &func);
  glGetFloatv(GL_ALPHA_TEST_REF, &ref);

  newState.bEnabled = glIsEnabled(GL_ALPHA_TEST);
  newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));
  newState.fRefValue = ref;

  return newState;
}

long GlGpuStateManager::SetAlphaTestState(AlphaTestState &state) {
  if (kErrNone == SetAlphaTest(state.bEnabled) &&
      kErrNone == SetAlphaTestFunc(state.cmpFunc, state.fRefValue)) {
    return kErrNone;
  }

  return kErrFailure;
}

long GlGpuStateManager::SetAlphaTest(bool enabled) {
  if (enabled == true) {
    glEnable(GL_ALPHA_TEST);
  } else {
    glDisable(GL_ALPHA_TEST);
  }

  return kErrNone;
}

long GlGpuStateManager::SetAlphaTestFunc(Comparison func, float ref) {
  glAlphaFunc(ConvertToGLEnum(COMPARISON_TABLE, func), ref);

  return kErrNone;
}

DepthTestState GlGpuStateManager::GetDepthTestState() {
  DepthTestState newState;
  GLboolean depthMask;
  int func = 0;

  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  glGetIntegerv(GL_DEPTH_FUNC, &func);

  newState.bEnabled = glIsEnabled(GL_DEPTH_TEST);
  newState.bDepthMask = depthMask;
  newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));

  return newState;
}

long GlGpuStateManager::SetDepthTestState(DepthTestState &state) {
  if (kErrNone == SetDepthTest(state.bEnabled) &&
      kErrNone == SetDepthTestFunc(state.cmpFunc)) {
    return kErrNone;
  }

  return kErrFailure;
}

long GlGpuStateManager::SetDepthTest(bool enabled) {
  if (enabled == true) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }

  return kErrNone;
}

long GlGpuStateManager::SetDepthTestFunc(Comparison func) {
  glDepthFunc(ConvertToGLEnum(COMPARISON_TABLE, func));

  return kErrNone;
}

BlendState GlGpuStateManager::GetBlendState() {
  BlendState newState;
  int sFactor = 0;
  int dFactor = 0;

  glGetIntegerv(GL_BLEND_SRC, &sFactor);
  glGetIntegerv(GL_BLEND_DST, &dFactor);

  newState.bEnabled = glIsEnabled(GL_BLEND);
  newState.srcFactor = static_cast<BlendFactor>(ConvertGLEnum(sFactor));
  newState.dstFactor = static_cast<BlendFactor>(ConvertGLEnum(dFactor));

  return newState;
}

long GlGpuStateManager::SetBlendState(BlendState &state) {
  if (kErrNone != SetBlending(state.bEnabled)) return kErrFailure;

  if (state.bEnabled) {
    GLenum sFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.srcFactor);
    GLenum dFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.dstFactor);
    glBlendFunc(sFactor, dFactor);
  }

  return kErrNone;
}

long GlGpuStateManager::SetBlending(bool enabled) {
  if (enabled == true) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
  return kErrNone;
}

Viewport3D GlGpuStateManager::GetViewportState() {
  int values[4];

  glGetIntegerv(GL_VIEWPORT, values);

  return Viewport3D(values[0], values[1], values[2], values[3], 0, 0, 0);
}

long GlGpuStateManager::SetViewportState(Viewport3D &state) {
  glViewport(state.ulX, state.ulY, state.ulWidth, state.ulHeight);

  return kErrNone;
}

Color GlGpuStateManager::GetColorState() {
  float color[4];

  glGetFloatv(GL_CURRENT_COLOR, color);

  return Color(color[0], color[1], color[2], color[3]);
}

long GlGpuStateManager::SetColorState(Color &colorState) {
  glColor4f(colorState.fRed, colorState.fGreen, colorState.fBlue,
            colorState.fA);

  return kErrNone;
}

long GlGpuStateManager::Set2DTextures(bool enabled) {
  if (enabled == true) {
    glEnable(GL_TEXTURE_2D);
  } else {
    glDisable(GL_TEXTURE_2D);
  }

  return kErrNone;
}

long GlGpuStateManager::Set2DRectTextures(bool enabled) {
  if (enabled == true) {
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
  } else {
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
  }

  return kErrNone;
}

long GlGpuStateManager::SetSampler(TextureSampler &sampler) {
  /* Check if anisotropy was set */
  if ((fabs(sampler.anisotropy - 0) < 1e-6)) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    sampler.anisotropy);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
  glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
  glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
  glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_R,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));

  return kErrNone;
}

long GlGpuStateManager::SetRectSampler(TextureSampler &sampler) {
  /* Check if anisotropy was set */
  if ((fabs(sampler.anisotropy - 0) < 1e-6)) {
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    sampler.anisotropy);
  }

  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                  ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                  ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
  glTexParameteri(
      GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
  glTexParameteri(
      GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
  glTexParameteri(
      GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_R,
      ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));

  return kErrNone;
}

long GlGpuStateManager::SetTextureEnvironment(TextureEnvMode &envMode) {
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
            ConvertToGLEnum(TEXTURE_ENVIRONMENT_TABLE, envMode.envMode));

  return kErrNone;
}

Matrix GlGpuStateManager::GetWorldViewMatrix() {
  Matrix result;

  glGetFloatv(GL_MODELVIEW_MATRIX, (float *)&result);

  return result;
}

Matrix GlGpuStateManager::GetProjectionMatrix() {
  Matrix result;

  glGetFloatv(GL_PROJECTION_MATRIX, (float *)&result);

  return result;
}

long GlGpuStateManager::SetWorldViewMatrix(Matrix &matrix) {
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf((float *)(&matrix));

  return kErrNone;
}

long GlGpuStateManager::SetProjectionMatrix(Matrix &matrix) {
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf((float *)(&matrix));

  return kErrNone;
}

long GlGpuStateManager::GetClearColorValue(float &red, float &green,
                                              float &blue, float &alpha) {
  GLfloat clr[4];
  glGetFloatv(GL_CLEAR, clr);
  red = clr[0];
  green = clr[1];
  blue = clr[2];
  alpha = clr[3];

  return kErrNone;
}

long GlGpuStateManager::SetClearColorValue(float red, float green,
                                              float blue, float alpha) {
  glClearColor(red, green, blue, alpha);
  return kErrNone;
}

long GlGpuStateManager::GetClearDepthValue(float &depth) {
  glGetFloatv(GL_DEPTH, &depth);
  return kErrNone;
}

long GlGpuStateManager::SetClearDepthValue(float depth) {
  glClearDepth(depth);
  return kErrNone;
}

long GlGpuStateManager::GetStencilClearValue(ulong &s) {
  int nS = 0;
  glGetIntegerv(GL_STENCIL, &nS);
  s = nS;
  return kErrNone;
}

long GlGpuStateManager::SetStencilClearValue(ulong s) {
  glClearStencil(s);
  return kErrNone;
}

long GlGpuStateManager::SetPolygonMode(FaceMode face, PolygonMode mode) {
  GLenum GLFace = ConvertToGLEnum(FACE_TABLE, face);
  GLenum GLMode = ConvertToGLEnum(POLYGON_MODE_TABLE, mode);

  glPolygonMode(GLFace, GLMode);
  return kErrNone;
}

long GlGpuStateManager::GetLineWidth(float &size) {
  glGetFloatv(GL_LINE_WIDTH, &size);
  return kErrNone;
}

long GlGpuStateManager::SetLineWidth(float size) {
  glLineWidth(size);
  return kErrNone;
}

long GlGpuStateManager::GetPointSize(float &size) {
  glGetFloatv(GL_POINT_SIZE, &size);
  return kErrNone;
}

long GlGpuStateManager::SetPointSize(float size) {
  glPointSize(size);
  return kErrNone;
}

long GlGpuStateManager::SetMaterail(bool enabled) {
  if (enabled) {
    glEnable(GL_COLOR_MATERIAL);
  } else
    glDisable(GL_COLOR_MATERIAL);

  return kErrNone;
}

long GlGpuStateManager::SetLight(bool enabled) {
  if (enabled) {
    glEnable(GL_LIGHTING);
  } else
    glDisable(GL_LIGHTING);

  return kErrNone;
}

long GlGpuStateManager::EnableDepthOffset(PolygonMode mode, bool enabled) {
  int tmp = 0;

  switch (mode) {
    case PM_POINT:
      tmp = GL_POLYGON_OFFSET_POINT;
      break;
    case PM_LINE:
      tmp = GL_POLYGON_OFFSET_LINE;
      break;
    case PM_FILL:
      tmp = GL_POLYGON_OFFSET_FILL;
      break;
    default:;
  }

  if (enabled) {
    glEnable(tmp);
  } else {
    glDisable(tmp);
  }

  return kErrNone;
}

long GlGpuStateManager::DepthOffsetParams(float rFactor, float dFactor) {
  glPolygonOffset(rFactor, dFactor);

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic
