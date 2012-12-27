#include "stdafx.h"
#include "cata_mapservicemgr.h"
#include "sde_datasourcemgr.h"
#include "smt_api.h"
#include "t_msg.h"
#include "cata_mapservicexcatalog.h"

#include <algorithm>

namespace Smt_XCatalog
{
	SmtMapServiceMgr* SmtMapServiceMgr::m_pSingleton = NULL;

	SmtMapServiceMgr* SmtMapServiceMgr::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtMapServiceMgr();
		}

		return m_pSingleton;
	}

	void SmtMapServiceMgr::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtMapServiceMgr::SmtMapServiceMgr(void)
	{
		m_pCurrentMapService = NULL;
		m_nIteratorIndex = 0;
		m_strMSVRCfg = "";
	}

	SmtMapServiceMgr::~SmtMapServiceMgr(void)
	{
		SaveMSVRCfg(m_strMSVRCfg.c_str());

		vector<SmtMapService*>::iterator iter = m_vMapServices.begin() ;

		while (iter != m_vMapServices.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}

		m_vMapServices.clear();

		m_pCurrentMapService = NULL;

		m_vMServiceCatalogPtrs.clear();
	}

	bool SmtMapServiceMgr::RegisterMServiceCatalog(void*pMServiceCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_vMServiceCatalogPtrs.begin(),m_vMServiceCatalogPtrs.end(),pMServiceCatalog); 

		if(iter ==m_vMServiceCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_vMServiceCatalogPtrs.push_back(pMServiceCatalog); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapServiceMgr::UnregisterMServiceCatalog(void*pMServiceCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_vMServiceCatalogPtrs.begin(),m_vMServiceCatalogPtrs.end(),pMServiceCatalog); 

		if(iter!=m_vMServiceCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_vMServiceCatalogPtrs.erase(iter); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapServiceMgr::UpdateMServiceCatalog(void)
	{
		SmtMapServiceXCatalog *pMServiceCatalog;
		vector<void*>::iterator iter = m_vMServiceCatalogPtrs.begin();
		while(iter!=m_vMServiceCatalogPtrs.end())
		{
			pMServiceCatalog = (SmtMapServiceXCatalog *)(*iter);
			pMServiceCatalog->UpdateMServiceCatalogTree();
			++iter;
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapServiceMgr::OpenMSVRCfg(const char *szCfgFile)
	{
		if (strlen(szCfgFile) == 0)
			return false;

		string strFileName = szCfgFile;
		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
			return false;

		string strExt = strFileName.substr(nPos+1,4);

		if (stricmp(strExt.c_str(),"msvr") != 0)
			return false;

		m_strMSVRCfg = strFileName;

		DeleteAll();

		ifstream infile;

		locale loc = locale::global(locale(".936"));
		infile.open(strFileName.c_str(),ios::out|ios::binary);
		locale::global(std::locale(loc));

		if (!infile.is_open())
		{
			return SMT_ERR_FAILURE;
		}

		//////////////////////////////////////////////////////////////////////////
		//head
		char szHead[6] = "msvr";
		infile.read((char*)(szHead),sizeof(char)*6);

		//content
		int nSVRs = 0;
		infile.read((char*)(&nSVRs),sizeof(int));

		char szName[MAX_NAME_LENGTH];
		char szFile[MAX_FILE_PATH];
		long lTHeight = 0,lTWidth = 0;
		long lZoomMax = 0,lZoomMin = 0;
		long lImageCode = -1;
		SmtMapService* pMapService = NULL;

		for (int i = 0; i < nSVRs;i++)
		{
			pMapService = new SmtMapService;
			m_vMapServices.push_back(pMapService);

			infile.read((char*)(szName),sizeof(char)*MAX_MAP_NAME);
			pMapService->SetName(szName);

			infile.read((char*)(szFile),sizeof(char)*MAX_FILE_PATH);
			pMapService->SetMapDoc(szFile);

			infile.read((char*)(szName),sizeof(char)*MAX_NAME_LENGTH);
			pMapService->SetProvider(szName);

			infile.read((char*)(szFile),sizeof(char)*MAX_FILE_PATH);
			//pMapService->SetTileCacheDir(szFile);

			infile.read((char*)(&lImageCode),sizeof(long));
			infile.read((char*)(&lTWidth),sizeof(long));
			infile.read((char*)(&lTHeight),sizeof(long));
			infile.read((char*)(&lZoomMin),sizeof(long));
			infile.read((char*)(&lZoomMax),sizeof(long));

			pMapService->SetTileSize(lTWidth,lTHeight);
			pMapService->SetImageCode(lImageCode);
			pMapService->SetZoomMax(lZoomMax);
			pMapService->SetZoomMin(lZoomMin);
		}

		if (m_vMapServices.size() > 0) 
			m_pCurrentMapService = m_vMapServices[m_vMapServices.size()-1];

		//////////////////////////////////////////////////////////////////////////

		infile.close();

		CreateMapService();

		UpdateMServiceCatalog();
		
		return true;
	}

	bool SmtMapServiceMgr::SaveMSVRCfg(const char *szCfgFile)
	{
		string strFileName = "";
		if (strlen(szCfgFile) == 0)
			strFileName = m_strMSVRCfg;
		else
			strFileName = szCfgFile;

		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
		{
			strFileName += ".msvr";
		}

		ofstream outfile;

		locale loc = locale::global(locale(".936"));
		outfile.open(strFileName.c_str(),ios::out|ios::binary);
		locale::global(std::locale(loc));

		if (!outfile.is_open())
		{
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		//head
		char szHead[6] = "msvr";
		outfile.write((char*)(szHead),sizeof(char)*6);

		//content
		int nSVRs = GetMapServiceCount();
		outfile.write((char*)(&nSVRs),sizeof(int));

		char szName[MAX_NAME_LENGTH];
		char szFile[MAX_FILE_PATH];
		long lTHeight = 0,lTWidth = 0;
		long lZoomMax = 0,lZoomMin = 0;
		long lImageCode = -1;
		SmtMapService* pMapService = NULL;
		vector<SmtMapService*>::iterator iter = m_vMapServices.begin() ;
		while (iter != m_vMapServices.end())
		{
			pMapService = (*iter);

			pMapService->GetTileSize(lTWidth,lTHeight);
			lImageCode = pMapService->GetImageCode();
			lZoomMax = pMapService->GetZoomMax();
			lZoomMin = pMapService->GetZoomMin();

			sprintf_s(szName,MAX_MAP_NAME,"%s",pMapService->GetName().c_str());
			outfile.write((char*)(szName),sizeof(char)*MAX_MAP_NAME);

			sprintf_s(szFile,MAX_FILE_PATH,"%s",pMapService->GetMapDoc().c_str());
			outfile.write((char*)(szFile),sizeof(char)*MAX_FILE_PATH);

			sprintf_s(szName,MAX_NAME_LENGTH,"%s",pMapService->GetProvider().c_str());
			outfile.write((char*)(szName),sizeof(char)*MAX_NAME_LENGTH);

			sprintf_s(szFile,MAX_FILE_PATH,"%s",pMapService->GetTileCacheDir().c_str());
			outfile.write((char*)(szFile),sizeof(char)*MAX_FILE_PATH);

			outfile.write((char*)(&lImageCode),sizeof(long));
			outfile.write((char*)(&lTWidth),sizeof(long));
			outfile.write((char*)(&lTHeight),sizeof(long));
			outfile.write((char*)(&lZoomMin),sizeof(long));
			outfile.write((char*)(&lZoomMax),sizeof(long));
			
			++iter;
		}
		//////////////////////////////////////////////////////////////////////////

		outfile.close();

		return true;
	}

	bool SmtMapServiceMgr::CreateMapService()
	{
		SmtMapService* pMapService = NULL;
		char szTileCacheUrl[MAX_FILE_PATH];
		vector<SmtMapService*>::iterator iter = m_vMapServices.begin() ;
		while (iter != m_vMapServices.end())
		{
			pMapService = (*iter);

			pMapService->Init();
			pMapService->Create();

			sprintf_s(szTileCacheUrl,MAX_FILE_PATH,"%s%s.ti",pMapService->GetTileCacheDir().c_str(),pMapService->GetName().c_str());

			pMapService->ConnectTileCache(szTileCacheUrl);

			++iter;
		}

		return true;
	}

	bool SmtMapServiceMgr::AppendMapService(SmtMapService *pMapService)
	{
		bool bRet = false;
		if (GetMapService(pMapService->GetName().c_str()) == NULL)
		{
			m_vMapServices.push_back(const_cast<SmtMapService *>(pMapService));

			m_pCurrentMapService = m_vMapServices[m_vMapServices.size()-1];

			bRet = true;
		}

		UpdateMServiceCatalog();

		return bRet;
	}

	bool SmtMapServiceMgr::RemoveMapService(const char *szMapServiceName)
	{
		bool bRet = true;
		vector<SmtMapService*>::iterator iter = m_vMapServices.begin() ;
		while (iter != m_vMapServices.end())
		{
			if (strcmp((*iter)->GetName().c_str(),szMapServiceName) == 0)
			{
				if ((*iter) == m_pCurrentMapService)
					m_pCurrentMapService = NULL;

				SMT_SAFE_DELETE(*iter);
				break;
			}
			iter++;
		}

		if (iter != m_vMapServices.end())
		{
			m_vMapServices.erase(iter);

			bRet = true;
		}

		UpdateMServiceCatalog();

		return bRet;
	}

	bool SmtMapServiceMgr::RemoveMapService(SmtMapService *pMapService)
	{
		return RemoveMapService(pMapService->GetName().c_str());
	}

	SmtMapService* SmtMapServiceMgr::GetMapService(int index)
	{
		if (index < 0 || index > (m_vMapServices.size()-1) )
			return NULL;

		return m_vMapServices[index];
	}

	const SmtMapService* SmtMapServiceMgr::GetMapService(int index) const
	{
		if (index < 0 || index > (m_vMapServices.size()-1) )
			return NULL;

		return m_vMapServices[index];
	}

	SmtMapService* SmtMapServiceMgr::GetMapService(const char *szMapServiceName)
	{
		SmtMapService* pMapService = NULL;
		vector<SmtMapService*>::iterator i = m_vMapServices.begin() ;

		while (i != m_vMapServices.end())
		{
			if (strcmp((*i)->GetName().c_str(),szMapServiceName) == 0)
			{
				pMapService =  (*i);
				break;
			}
			++i;
		}

		return pMapService;
	}

	const SmtMapService* SmtMapServiceMgr::GetMapService(const char *szMapServiceName) const
	{
		SmtMapService* pMapService = NULL;
		vector<SmtMapService*>::const_iterator i = m_vMapServices.begin() ;

		while (i != m_vMapServices.end())
		{
			if (strcmp((*i)->GetName().c_str(),szMapServiceName) == 0)
			{
				pMapService =  (*i);
				break;
			}
			++i;
		}

		return pMapService;
	}

	void SmtMapServiceMgr::MoveFirst(void) const 
	{
		m_nIteratorIndex = 0;
	}

	void SmtMapServiceMgr::MoveNext(void) const 
	{
		if (m_nIteratorIndex < m_vMapServices.size())
			m_nIteratorIndex++;
	}

	void SmtMapServiceMgr::MoveLast(void) const 
	{
		m_nIteratorIndex = m_vMapServices.size() -1 ;
	}

	void SmtMapServiceMgr::Delete(void)
	{
		SmtMapService *pSmtMapService = m_vMapServices[m_nIteratorIndex];

		m_vMapServices.erase( m_vMapServices.begin() + m_nIteratorIndex );

		SMT_SAFE_DELETE(pSmtMapService);

	}

	bool SmtMapServiceMgr::IsEnd(void) const 
	{
		return (m_nIteratorIndex == m_vMapServices.size());
	}

	void SmtMapServiceMgr::DeleteAll(void)
	{
		vector<SmtMapService*>::iterator iter = m_vMapServices.begin() ;

		while (iter != m_vMapServices.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}
		m_vMapServices.clear();

		m_pCurrentMapService = NULL;
	}

	SmtMapService * SmtMapServiceMgr::GetMapService()
	{
		return GetMapService(m_nIteratorIndex);
	}

	const SmtMapService * SmtMapServiceMgr::GetMapService() const 
	{
		return GetMapService(m_nIteratorIndex);
	}
}