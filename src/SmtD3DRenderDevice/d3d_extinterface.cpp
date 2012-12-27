#include "d3d_3drenderdevice.h"


BOOL APIENTRY DllMain(HANDLE hModule,DWORD ul_reson_for_call,LPVOID lpReserved)
{
	switch(ul_reson_for_call)
	{
	   case DLL_PROCESS_ATTACH:
		   break;
	   case DLL_THREAD_ATTACH:		 
		   break;
       case DLL_THREAD_DETACH:
		   break;
       case DLL_PROCESS_DETACH:
		   break;
	}
	return true;
}

namespace Smt_3Drd
{

	HRESULT Create3DRenderDevice(HINSTANCE hDLL, Smt3DRenderDevice* &  pDevice) 
	{
		if(!pDevice) 
		{
			pDevice = new SmtD3DRenderDevice(hDLL);
			return SMT_OK;
		}

		return SMT_FALSE;
	}

	HRESULT Release3DRenderDevice(Smt3DRenderDevice* & pDevice) 
	{
		if(!pDevice) 
		{
			return SMT_FALSE;
		}
		delete pDevice;
		pDevice = NULL;

		return SMT_OK;
	}
}