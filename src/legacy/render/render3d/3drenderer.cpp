#include "legacy/render/render3d/3drenderer.h"

namespace render
{
	Smt3DRenderer::Smt3DRenderer(HINSTANCE hInst) 
	{
		m_hInst   = hInst;
		m_pDevice = NULL;
		m_hDLL    = NULL;
	}

	Smt3DRenderer::~Smt3DRenderer(void) 
	{ 
		Release(); 
	}

	long Smt3DRenderer::CreateDevice(const char *chAPI) 
	{
		char buffer[300];
		
		if (strcmp(chAPI, "OpenGL") == 0) 
		{

#ifdef _DEBUG
			m_hDLL = LoadLibrary("legacy_render_d.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading legacy_render_d.dll failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_FALSE;
			}
#else
			m_hDLL = LoadLibrary("legacy_render.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading legacy_render.dll failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_FALSE; 
			}
#endif
		}
		else if (strcmp(chAPI, "Direct3D") == 0) 
		{
			// No Direct3D device DLL in the reorg topology (only render_gl).
			::MessageBox(NULL, "Direct3D render device is not supported.",
			             "SmartGis - error", MB_OK | MB_ICONERROR);
			return SMT_FALSE;
		}
		else 
		{
			_snprintf(buffer, 300, "API '%s' not yet supported.", chAPI);
			::MessageBox(NULL, buffer, "SmartGis - error", MB_OK |MB_ICONERROR);
			return SMT_FALSE;
		}
		
		_Create3DRenderDevice _CreateRDev = 0;
		HRESULT hr;
		
		_CreateRDev = (_Create3DRenderDevice)GetProcAddress(m_hDLL,"Create3DRenderDevice");

		hr = _CreateRDev(m_hDLL, m_pDevice);

		if(FAILED(hr))
		{
			::MessageBox(NULL,"Create3DRenderDevice() from lib failed.","SmartGis - error", MB_OK | MB_ICONERROR);
			m_pDevice = NULL;

			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	} 

	void Smt3DRenderer::Release(void) 
	{
		_Release3DRenderDevice _ReleaseRDev = 0;
		HRESULT hr;
		
		if (m_hDLL) 
		{
			_ReleaseRDev = (_Release3DRenderDevice)GetProcAddress(m_hDLL, "Release3DRenderDevice");
		}
		
		if (m_pDevice) 
		{
			hr = _ReleaseRDev(m_pDevice);
			if(FAILED(hr))
			{
				m_pDevice = NULL;
			}
		}
	}
}