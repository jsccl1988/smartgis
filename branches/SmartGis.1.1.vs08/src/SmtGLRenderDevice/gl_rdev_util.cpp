#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	bool SmtGLRenderDevice::IsExtensionSupported(string extension)
	{
		char *ext = (char*)glGetString(GL_EXTENSIONS);
		if (NULL != ext &&
			strstr(ext, extension.c_str()) != NULL)
		{
			return true;
		}

		return false;
	}

	void* SmtGLRenderDevice::GetProcAddress(string name)
	{
		return wglGetProcAddress ( name.c_str() );
	}

	SmtGPUStateManager* SmtGLRenderDevice::GetStateManager()
	{
		return m_pStateManager;
	}

	Smt3DDeviceCaps* SmtGLRenderDevice::GetDeviceCaps()
	{
		return m_pDeviceCaps;
	}
}