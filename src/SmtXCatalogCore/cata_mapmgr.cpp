#include "stdafx.h"
#include "cata_mapmgr.h"
#include "sde_datasourcemgr.h"
#include "smt_api.h"
#include "vw_2dxview.h"
#include "cata_mapdocxcatalog.h"
#include "t_msg.h"
#include <algorithm>

using namespace Smt_SDEDevMgr;
using namespace Smt_GIS;
using namespace Smt_XView;

namespace Smt_XCatalog
{
	SmtMapMgr* SmtMapMgr::m_pSingleton = NULL;

	SmtMapMgr* SmtMapMgr::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtMapMgr();
		}

		return m_pSingleton;
	}

	void SmtMapMgr::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtMapMgr::SmtMapMgr(void)
	{
		m_pSmtMap = NULL;
		m_strMDocPath = "";
	}

	SmtMapMgr::~SmtMapMgr(void)
	{
		SaveMap();

		SMT_SAFE_DELETE(m_pSmtMap);
		m_v2DXViewPtrs.clear();
		m_vMapCatalogPtrs.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	SmtMap *SmtMapMgr::GetSmtMapPtr(void)
	{
		return m_pSmtMap;
	}

	const SmtMap * SmtMapMgr::GetSmtMapPtr(void) const
	{
		return m_pSmtMap;
	}

	SmtMap& SmtMapMgr::GetSmtMap(void)
	{
		return *m_pSmtMap;
	}

	const SmtMap& SmtMapMgr::GetSmtMap(void) const
	{
		return *m_pSmtMap;
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtMapMgr::NewMap(SmtMap *&pMap,const char *szMapName)
	{
		if (NULL != pMap)
			return false;

		pMap = new SmtMap();
		pMap->SetMapName(szMapName);

		return true;
	}

	bool SmtMapMgr::OpenMap(SmtMap *pMap,const char *szMapFile)
	{
		string strFileName = szMapFile;
		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
			return false;

		string strExt = strFileName.substr(nPos+1,4);

		if (stricmp(strExt.c_str(),"mdoc") != 0)
			return false;

		if (NULL == pMap)
			return false;

		//1.clear map
		pMap->DeleteAll();

		//2.open map
		if (strlen(szMapFile) == 0)
			return false;

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
		char szHead[6] = "mdoc";
		infile.read((char*)(szHead),sizeof(char)*6);

		//content
		char szMapName[MAX_MAP_NAME];

		infile.read((char*)(szMapName),sizeof(char)*MAX_MAP_NAME);
		pMap->SetMapName(szMapName);

		/*Envelope	env;
		infile.read((char*)(&env),sizeof(Envelope));*/

		int nLyrs = 0;
		infile.read((char*)(&nLyrs),sizeof(int));

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr(); 

		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0 ; i < nLyrs ; i++)
		{
			infile.read((char*)(&info),sizeof(SmtDataSourceInfo));
			infile.read((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);

			SmtDataSource *pDS = pDSMgr->GetDataSource(info.szName);
			if (NULL == pDS)
			{
				pDS = pDSMgr->CreateDataSource(info);
			}

			if (NULL != pDS && pDS->Open() && pDS->GetLayerCount() > 0)
			{
				SmtLayer *pLayer = NULL;
				pLayer = pMap->GetLayer(szLayerName);
				if (pLayer == NULL)
				{
					SmtLayerInfo lyrInfo;
					pDS->GetLayerInfo(lyrInfo,szLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
						pLayer = pDS->OpenRasterLayer(szLayerName);
					else 
						pLayer = pDS->OpenVectorLayer(szLayerName);	

					if (pLayer && pMap->AddLayer(pLayer))
					{	
						;
					}	
				}	

				pDS->Close();
			}
		}

		//////////////////////////////////////////////////////////////////////////

		infile.close();

		return true;
	}

	bool SmtMapMgr::SaveMapAs(SmtMap *pMap,const char *szFilePath)
	{
		if(NULL == pMap)
			return false;

		if (strlen(szFilePath) == 0)
			return false;

		string strFileName = szFilePath;
		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
		{
			strFileName += ".mdoc";
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
		char szHead[6] = "mdoc";
		outfile.write((char*)(szHead),sizeof(char)*6);

		//content
		char szMapName[MAX_MAP_NAME];
		sprintf_s(szMapName,MAX_MAP_NAME,"%s",pMap->GetMapName());
		outfile.write((char*)(szMapName),sizeof(char)*MAX_MAP_NAME);

		/*Envelope	env;
		m_pSmtMap->GetEnvelope(env);
		outfile.write((char*)(&env),sizeof(Envelope));*/

		int nLyrs = pMap->GetLayerCount();
		outfile.write((char*)(&nLyrs),sizeof(int));

		const SmtDataSource *pDS = NULL;
		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		SmtLayer *pLayer = NULL;

		pMap->MoveFirst();
		while(!pMap->IsEnd())
		{
			pLayer = pMap->GetLayer();
			pDS = pLayer->GetDataSource();
			pDS->GetInfo(info);
			strcpy_s(szLayerName,MAX_LAYER_NAME,pLayer->GetLayerName());

			outfile.write((char*)(&info),sizeof(SmtDataSourceInfo));
			outfile.write((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);
			pMap->MoveNext();
		}
		//////////////////////////////////////////////////////////////////////////

		outfile.close();

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapMgr::OpenMap(const char *szMapFile)
	{
		string strFileName = szMapFile;
		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
			return false;

		string strExt = strFileName.substr(nPos+1,4);

		if (stricmp(strExt.c_str(),"mdoc") != 0)
			return false;

		CloseMap();

		//1.new map file doc
		m_pSmtMap = new SmtMap();

		//2.open map
		if (strlen(szMapFile) == 0)
			return false;

		m_strMDocPath = szMapFile;

		ifstream infile;

		locale loc = locale::global(locale(".936"));
		infile.open(m_strMDocPath.c_str(),ios::out|ios::binary);
		locale::global(std::locale(loc));

		if (!infile.is_open())
		{
			return SMT_ERR_FAILURE;
		}

		//////////////////////////////////////////////////////////////////////////
		//head
		char szHead[6] = "mdoc";
		infile.read((char*)(szHead),sizeof(char)*6);

		//content
		char szMapName[MAX_MAP_NAME];
		
		infile.read((char*)(szMapName),sizeof(char)*MAX_MAP_NAME);
		m_pSmtMap->SetMapName(szMapName);

		/*Envelope	env;
		infile.read((char*)(&env),sizeof(Envelope));*/
		
		int nLyrs = 0;
		infile.read((char*)(&nLyrs),sizeof(int));

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr(); 

		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0 ; i < nLyrs ; i++)
		{
			infile.read((char*)(&info),sizeof(SmtDataSourceInfo));
			infile.read((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);

			SmtDataSource *pDS = pDSMgr->GetDataSource(info.szName);

			if (NULL != pDS && pDS->Open() && pDS->GetLayerCount() > 0)
			{
				SmtLayer *pLayer = NULL;
				pLayer = GetLayer(szLayerName);
				if (pLayer == NULL)
				{
					SmtLayerInfo lyrInfo;
					pDS->GetLayerInfo(lyrInfo,szLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
						pLayer = pDS->OpenRasterLayer(szLayerName);
					else 
						pLayer = pDS->OpenVectorLayer(szLayerName);	

					if (pLayer && m_pSmtMap)
					{	
						m_pSmtMap->AddLayer(pLayer);
						m_pSmtMap->SetActiveLayer(pLayer->GetLayerName());
					}	
				}	

				pDS->Close();
			}
		}

		//////////////////////////////////////////////////////////////////////////

		infile.close();

		UpdateMapCatalog();
		Update2DXView();

		SmtListenerMsg param;
		param.hSrcWnd = NULL;
		SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMRESTORE,NULL),param);

		return true;
	}

	bool SmtMapMgr::NewMap(const char *szMapName)
	{
		CloseMap();

		m_pSmtMap = new SmtMap();
		m_pSmtMap->SetMapName(szMapName);

		UpdateMapCatalog();
		Update2DXView();

		return true;
	}

	bool SmtMapMgr::CloseMap()
	{
		if (NULL != m_pSmtMap &&
			::MessageBox(NULL,"Save Map Document?","SmartGis - Tip",MB_YESNO) == IDYES)
		{
			SaveMap();
		}
		
		SMT_SAFE_DELETE(m_pSmtMap);

		UpdateMapCatalog();
		Update2DXView();

		return true;
	}

	bool SmtMapMgr::SaveMap()
	{
		if(NULL == m_pSmtMap)
			return false;

		if (m_strMDocPath == "")
		{
			string strAppPath = GetAppPath();
			char szParentPath[_MAX_DIR];
			char szBuf[_MAX_DIR];

			GetParentDictory(strAppPath.c_str(),szParentPath,1);

			sprintf_s(szBuf,_MAX_DIR,"%sdata source\\%s.mdoc",szParentPath,m_pSmtMap->GetMapName());

			m_strMDocPath =szBuf;
		}

		return SaveMapAs(m_strMDocPath.c_str());
	}

	bool SmtMapMgr::SaveMapAs(const char *szFilePath)
	{
		if(NULL == m_pSmtMap)
			return false;

		if (strlen(szFilePath) == 0)
			return false;

		string strFileName = szFilePath;
		int nPos = strFileName.rfind('.');
		if (nPos == string::npos)
		{
			strFileName += ".mdoc";
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
		char szHead[6] = "mdoc";
		outfile.write((char*)(szHead),sizeof(char)*6);

		//content
		char szMapName[MAX_MAP_NAME];
		sprintf_s(szMapName,MAX_MAP_NAME,"%s",m_pSmtMap->GetMapName());
		outfile.write((char*)(szMapName),sizeof(char)*MAX_MAP_NAME);

		/*Envelope	env;
		m_pSmtMap->GetEnvelope(env);
		outfile.write((char*)(&env),sizeof(Envelope));*/
		
		int nLyrs = m_pSmtMap->GetLayerCount();
		outfile.write((char*)(&nLyrs),sizeof(int));

		const SmtDataSource *pDS = NULL;
		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		SmtLayer *pLayer = NULL;

		m_pSmtMap->MoveFirst();
		while(!m_pSmtMap->IsEnd())
		{
			pLayer = m_pSmtMap->GetLayer();
			pDS = pLayer->GetDataSource();
			pDS->GetInfo(info);
			strcpy_s(szLayerName,MAX_LAYER_NAME,pLayer->GetLayerName());

			outfile.write((char*)(&info),sizeof(SmtDataSourceInfo));
			outfile.write((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);
			m_pSmtMap->MoveNext();
		}
		//////////////////////////////////////////////////////////////////////////

		outfile.close();

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapMgr::Register2DXView(void*p2DXView)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v2DXViewPtrs.begin(),m_v2DXViewPtrs.end(),p2DXView); 

		if(iter ==m_v2DXViewPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v2DXViewPtrs.push_back(p2DXView); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapMgr::Unregister2DXView(void*p2DXView)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v2DXViewPtrs.begin(),m_v2DXViewPtrs.end(),p2DXView); 

		if(iter!=m_v2DXViewPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v2DXViewPtrs.erase(iter); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapMgr::Update2DXView(void)
	{
		Smt2DXView *p2DXView;
		vector<void*>::iterator iter = m_v2DXViewPtrs.begin();
		while(iter!=m_v2DXViewPtrs.end())
		{
			p2DXView = (Smt2DXView *)(*iter);
			p2DXView->SetOperMap(m_pSmtMap);
			++iter;
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapMgr::RegisterMapCatalog(void*pMapCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_vMapCatalogPtrs.begin(),m_vMapCatalogPtrs.end(),pMapCatalog); 

		if(iter ==m_vMapCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_vMapCatalogPtrs.push_back(pMapCatalog); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapMgr::UnregisterMapCatalog(void*pMapCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_vMapCatalogPtrs.begin(),m_vMapCatalogPtrs.end(),pMapCatalog); 

		if(iter!=m_vMapCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_vMapCatalogPtrs.erase(iter); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtMapMgr::UpdateMapCatalog(void)
	{
		SmtMapDocXCatalog *pMapCatalog;
		vector<void*>::iterator iter = m_vMapCatalogPtrs.begin();
		while(iter!=m_vMapCatalogPtrs.end())
		{
			pMapCatalog = (SmtMapDocXCatalog *)(*iter);
			pMapCatalog->UpdateMapTree();
			++iter;
		}

		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtMapMgr::AppendLayer(SmtLayer *pLayer)
	{
		bool bRet = true;

		if (m_pSmtMap)
		{
			bRet =  m_pSmtMap->AddLayer(pLayer);
			m_pSmtMap->SetActiveLayer(pLayer->GetLayerName());

			UpdateMapCatalog();
		}     

		return bRet;
	}

	bool SmtMapMgr::DeleteLayer(const char *szName)
	{
		bool bRet = false;
		if (m_pSmtMap)
		{
			bRet = m_pSmtMap->DeleteLayer(szName);

			UpdateMapCatalog();
		}

		return bRet;
	}

	SmtLayer* SmtMapMgr::GetLayer(const char *szName)
	{
		SmtLayer * pLayer = NULL;
		if (m_pSmtMap)
		{
			pLayer = m_pSmtMap->GetLayer(szName);
		}
		return pLayer;
	}

	SmtLayer* SmtMapMgr::GetLayer(int index)
	{
		SmtLayer * pLayer = NULL;
		if (m_pSmtMap)
		{
			pLayer = m_pSmtMap->GetLayer(index);
		}
		return pLayer;
	}


	bool SmtMapMgr::SetActiveLayer(const char *szName)
	{
		bool bRet = false;
		if (m_pSmtMap)
		{
			m_pSmtMap->SetActiveLayer(szName);
			bRet = true;
		}

		return bRet;
	}

	SmtLayer* SmtMapMgr::GetActiveLayer(void)
	{
		SmtLayer * pLayer = NULL;
		if (m_pSmtMap)
		{
			pLayer = m_pSmtMap->GetActiveLayer();
		}

		return pLayer;
	}

	bool SmtMapMgr::AppendFeature(SmtFeature *pFeature,bool bIsClone)
	{
		bool bRet = false;
		if (m_pSmtMap)
		{
			bRet = m_pSmtMap->AppendFeature(pFeature,bIsClone);
		}

		return bRet;
	}
}