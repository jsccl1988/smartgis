// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/gl/caps/device_caps.h"

#include "legacy/render/rhi3d/impl/gl/host/render_device.h"

namespace render {

SmtGLDeviceCaps::SmtGLDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice)
    : Smt3DDeviceCaps(p3DRenderDevice) {}

SmtGLDeviceCaps::~SmtGLDeviceCaps() = default;

bool SmtGLDeviceCaps::IsVSyncSupported() {
  return static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("WGL_EXT_swap_control");
}

bool SmtGLDeviceCaps::IsAnisotropySupported() {
  return static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("GL_EXT_texture_filter_anisotropic");
}

bool SmtGLDeviceCaps::IsVBOSupported() {
  return static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("GL_ARB_vertex_buffer_object");
}

bool SmtGLDeviceCaps::IsMipMapsSupported() { return false; }

bool SmtGLDeviceCaps::IsFBOSupported() {
  return static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("EXT_framebuffer_object");
}

bool SmtGLDeviceCaps::IsGLSLSupported() {
  auto* dev = static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice);
  return dev->IsExtensionSupported("GL_ARB_shading_language_100") &&
         dev->IsExtensionSupported("GL_ARB_shader_objects") &&
         dev->IsExtensionSupported("GL_ARB_vertex_shader") &&
         dev->IsExtensionSupported("GL_ARB_fragment_shader");
}

bool SmtGLDeviceCaps::IsMultiTextureSupported() {
  return static_cast<SmtGLRenderDevice*>(m_p3DRenderDevice)
      ->IsExtensionSupported("ARB_multitexture");
}

int SmtGLDeviceCaps::GetTextureSlotsCount() {
  int maxTextureUnits = 0;
  glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTextureUnits);
  return maxTextureUnits;
}

int SmtGLDeviceCaps::GetMaxColorAttachments() { return -1; }

float SmtGLDeviceCaps::GetMaxAnisotropy() {
  float maxLevel = 0.0f;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxLevel);
  return maxLevel;
}

}  // namespace render
