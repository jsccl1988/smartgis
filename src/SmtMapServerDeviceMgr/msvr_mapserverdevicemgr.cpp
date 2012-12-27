#include "msvr_mapserverdevicemgr.h"
#include "msvr_mapserverdevice111.h"

using namespace Smt_Core;

namespace Smt_MapServer
{
	SmtMapServerDeviceMgr* SmtMapServerDeviceMgr::m_pSingleton = NULL;

	SmtMapServerDeviceMgr* SmtMapServerDeviceMgr::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtMapServerDeviceMgr();
		}

		return m_pSingleton;
	}

	void SmtMapServerDeviceMgr::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtMapServerDevice * SmtMapServerDeviceMgr::CreateMapServerDevice(const char *szMapServerDevice)
	{
		SmtMapServerDevice *pMapServerDevice = NULL;
	
		if (strcmp(szMapServerDevice, "SmtMapServerDevice1.1.1") == 0) 
		{
			pMapServerDevice = new SmtMapServerDevice111();
		}

		return pMapServerDevice;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtMapServerDeviceMgr::DestoryMapServerDevice(SmtMapServerDevice *&pMapServerDevice)
	{
		SMT_SAFE_DELETE(pMapServerDevice);
	}

	SmtMapServerDeviceMgr::SmtMapServerDeviceMgr(void)
	{
		
	}

	SmtMapServerDeviceMgr::~SmtMapServerDeviceMgr(void)
	{
		
	}

}