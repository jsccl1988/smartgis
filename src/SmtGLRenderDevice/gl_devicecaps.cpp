#include "gl_devicecaps.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtGLDeviceCaps::SmtGLDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice):Smt3DDeviceCaps(p3DRenderDevice)
	{
		;
	}

	bool SmtGLDeviceCaps::IsVSyncSupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("WGL_EXT_swap_control");
	}

	bool SmtGLDeviceCaps::IsAnisotropySupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_EXT_texture_filter_anisotropic");
	}

	bool SmtGLDeviceCaps::IsVBOSupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_ARB_vertex_buffer_object");
	}

	bool SmtGLDeviceCaps::IsMipMapsSupported()
	{
		return false;
		//return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_SGIS_generate_mipmap");
	}

	bool SmtGLDeviceCaps::IsFBOSupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("EXT_framebuffer_object");
	}

	bool SmtGLDeviceCaps::IsGLSLSupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_ARB_shading_language_100") &&
			((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_ARB_shader_objects") &&
			((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_ARB_vertex_shader") &&
			((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("GL_ARB_fragment_shader");
	}

	bool SmtGLDeviceCaps::IsMultiTextureSupported()
	{
		return ((SmtGLRenderDevice*)m_p3DRenderDevice)->IsExtensionSupported("ARB_multitexture");
	}

	int SmtGLDeviceCaps::GetTextureSlotsCount()
	{
		int maxTextureUnits = 0;
		glGetIntegerv (GL_MAX_TEXTURE_UNITS, &maxTextureUnits);
		return maxTextureUnits;
	}

	int SmtGLDeviceCaps::GetMaxColorAttachments()
	{
		//return FBOFunctions->getMaxColorAttachments();
		return -1;
	}

	float SmtGLDeviceCaps::GetMaxAnisotropy()
	{
		float maxLevel = 0.0;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxLevel);
		return maxLevel;
	}

}