#include "rd_renderer.h"

namespace Smt_Rd
{
	SmtRenderer::SmtRenderer(HINSTANCE hInst)
	{
		m_hInst   = hInst;
		m_hDLL	  = NULL;
		m_pDevice = NULL;
	}

	SmtRenderer::~SmtRenderer(void)
	{
         Release(); 
	}

	int SmtRenderer::CreateDevice(const char *chAPI)
	{
		char buffer[300];

		if (strcmp(chAPI, "SmtGdiSimpleRenderDevice") == 0) 
		{

#ifdef _DEBUG
			m_hDLL = LoadLibrary("SmtGdiSimpleRenderDeviceD.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading SmtGdiSimpleRenderDeviceD.dll from lib failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_ERR_FAILURE;
			}
#else
			m_hDLL = LoadLibrary("SmtGdiSRenderDevice.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading SmtGdiSRenderDevice.dll from lib failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_ERR_FAILURE; 
			}
#endif
		}
		else if (strcmp(chAPI, "SmtGdiRenderDevice") == 0) 
		{

#ifdef _DEBUG
			m_hDLL = LoadLibrary("SmtGdiRenderDevice.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading SmtGdiRenderDeviceD.dll from lib failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_ERR_FAILURE;
			}
#else
			m_hDLL = LoadLibrary("SmtGdiRenderDevice.dll");
			if(!m_hDLL) 
			{
				::MessageBox(NULL,"Loading SmtGdiRenderDevice.dll from lib failed.","SmartGis - error", MB_OK | MB_ICONERROR);
				return SMT_ERR_FAILURE; 
			}
#endif
		}
		else 
		{
			_snprintf(buffer, 300, "API '%s' not yet supported.", chAPI);
			::MessageBox(NULL, buffer, "SmartGis - error", MB_OK |MB_ICONERROR);
			return SMT_FALSE;
		}

		_CreateRenderDevice _CreateRenderDev = 0;
		HRESULT hr;

		_CreateRenderDev = (_CreateRenderDevice)GetProcAddress(m_hDLL,"CreateRenderDevice");

		if (NULL == _CreateRenderDev)
			return SMT_ERR_FAILURE;
		 
		hr = _CreateRenderDev(m_hDLL, m_pDevice);

		if(FAILED(hr))
		{
			::MessageBox(NULL,"CreateRenderDevice() from lib failed.","SmtGis - error", MB_OK | MB_ICONERROR);
			m_pDevice = NULL;

			return SMT_ERR_FAILURE;
		}

       return SMT_ERR_NONE;
	}

	void  SmtRenderer::Release(void)
	{
		_DestroyRenderDevice _ReleaseRenderDev = 0;

		if (m_hDLL) 
		{
			_ReleaseRenderDev = (_DestroyRenderDevice)GetProcAddress(m_hDLL, "DestroyRenderDevice");
		}

		if (m_pDevice && _ReleaseRenderDev) 
		{
			_ReleaseRenderDev(m_pDevice);
		}
	} 
}