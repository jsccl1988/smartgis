#include "d3d_devicecaps.h"
#include "d3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtD3DDeviceCaps::SmtD3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice,IDirect3D9 *pD3D):Smt3DDeviceCaps(p3DRenderDevice)
	{
		pD3D->GetDeviceCaps( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &m_D3DCaps );
	}

	bool SmtD3DDeviceCaps::IsVSyncSupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsAnisotropySupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsVBOSupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsMipMapsSupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsFBOSupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsGLSLSupported()
	{
		return false;
	}

	bool SmtD3DDeviceCaps::IsMultiTextureSupported()
	{
		return false;
	}

	int SmtD3DDeviceCaps::GetTextureSlotsCount()
	{
		return -1;
	}

	int SmtD3DDeviceCaps::GetMaxColorAttachments()
	{
		return -1;
	}

	float SmtD3DDeviceCaps::GetMaxAnisotropy()
	{
		return 0.1;
	}

}