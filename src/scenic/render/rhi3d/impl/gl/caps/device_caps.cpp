// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/caps/device_caps.h"

#include "scenic/render/rhi3d/impl/gl/host/render_device.h"

namespace scenic {
namespace detail {

GlDeviceCaps::GlDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
    : DeviceCaps3d(p3DRenderDevice) {}

GlDeviceCaps::~GlDeviceCaps() = default;

bool GlDeviceCaps::IsVSyncSupported() {
  return static_cast<GlRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("WGL_EXT_swap_control");
}

bool GlDeviceCaps::IsAnisotropySupported() {
  return static_cast<GlRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("GL_EXT_texture_filter_anisotropic");
}

bool GlDeviceCaps::IsVBOSupported() {
  return static_cast<GlRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("GL_ARB_vertex_buffer_object");
}

bool GlDeviceCaps::IsMipMapsSupported() { return false; }

bool GlDeviceCaps::IsFBOSupported() {
  return static_cast<GlRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("EXT_framebuffer_object");
}

bool GlDeviceCaps::IsGLSLSupported() {
  auto* dev = static_cast<GlRenderDevice*>(m_p3DRenderDevice);
  return dev->IsExtensionSupported("GL_ARB_shading_language_100") &&
         dev->IsExtensionSupported("GL_ARB_shader_objects") &&
         dev->IsExtensionSupported("GL_ARB_vertex_shader") &&
         dev->IsExtensionSupported("GL_ARB_fragment_shader");
}

bool GlDeviceCaps::IsMultiTextureSupported() {
  return static_cast<GlRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("ARB_multitexture");
}

int GlDeviceCaps::GetTextureSlotsCount() {
  int maxTextureUnits = 0;
  glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTextureUnits);
  return maxTextureUnits;
}

int GlDeviceCaps::GetMaxColorAttachments() { return -1; }

float GlDeviceCaps::GetMaxAnisotropy() {
  float maxLevel = 0.0f;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxLevel);
  return maxLevel;
}

}  // namespace detail
}  // namespace scenic
