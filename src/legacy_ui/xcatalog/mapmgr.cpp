#include "stdafx.h"
#include "legacy_ui/xcatalog/mapmgr.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "base/core/api.h"
#include "legacy_ui/xcatalog/mapdocxcatalog.h"
#include "legacy_tool/group/defs.h"
#include "legacy_tool/t_msg.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <algorithm>

using namespace sdb;
using namespace sdb;

namespace ui
{
	SmtMapMgr* SmtMapMgr::m_pSingleton = NULL;

	SmtMapMgr* SmtMapMgr::get_singleton_ptr(void)
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
		m_fn2DXViewNotify = NULL;
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

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr(); 

		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0 ; i < nLyrs ; i++)
		{
			infile.read((char*)(&info),sizeof(SmtDataSourceInfo));
			infile.read((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);

			SmtDataSource leftover = pDSMgr->GetDataSource(info.szName);
			if (!leftover)
			{
				leftover = pDSMgr->CreateDataSource(info);
			}

			if (leftover && leftover.Open() && leftover.GetLayerCount() > 0)
			{
				if (pMap->GetLayer(szLayerName) == NULL &&
					pMap->GetOgrLayer(szLayerName) == NULL)
				{
					SmtLayerInfo lyrInfo;
					leftover.GetLayerInfo(lyrInfo,szLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
					{
						SmtLayer *pLayer = leftover.OpenRasterLayer(szLayerName);
						if (pLayer)
							pMap->AddLayer(pLayer);
					}
					else
					{
						OGRLayer *vl = leftover.OpenVectorLayer(szLayerName);
						if (vl)
							pMap->AddLayer(vl);
					}
				}	

				leftover.Close();
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
		m_pSmtMap->get_envelope(env);
		outfile.write((char*)(&env),sizeof(Envelope));*/

		int nLyrs = pMap->GetLayerCount();
		outfile.write((char*)(&nLyrs),sizeof(int));

		SmtDataSource leftover;
		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0; i < nLyrs; ++i)
		{
			leftover = SmtDataSource();
			if (OGRLayer *ogr = pMap->GetOgrLayer(i))
			{
				leftover = SmtDataSource(ogr->GetDataset());
				strcpy_s(szLayerName,MAX_LAYER_NAME,ogr->GetName());
			}
			else if (SmtLayer *pLayer = pMap->GetLeftoverLayer(i))
			{
				leftover = SmtDataSource(pLayer->GetDataSource());
				strcpy_s(szLayerName,MAX_LAYER_NAME,pLayer->GetLayerName());
			}

			leftover.GetInfo(info);
			outfile.write((char*)(&info),sizeof(SmtDataSourceInfo));
			outfile.write((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);
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

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr(); 

		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0 ; i < nLyrs ; i++)
		{
			infile.read((char*)(&info),sizeof(SmtDataSourceInfo));
			infile.read((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);

			SmtDataSource leftover = pDSMgr->GetDataSource(info.szName);

			if (leftover && leftover.Open() && leftover.GetLayerCount() > 0)
			{
				if (GetLayer(szLayerName) == NULL &&
					m_pSmtMap && m_pSmtMap->GetOgrLayer(szLayerName) == NULL)
				{
					SmtLayerInfo lyrInfo;
					leftover.GetLayerInfo(lyrInfo,szLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
					{
						SmtLayer *pLayer = leftover.OpenRasterLayer(szLayerName);
						if (pLayer && m_pSmtMap)
						{
							m_pSmtMap->AddLayer(pLayer);
							m_pSmtMap->SetActiveLayer(pLayer->GetLayerName());
						}
					}
					else if (m_pSmtMap)
					{
						OGRLayer *vl = leftover.OpenVectorLayer(szLayerName);
						if (vl)
						{
							m_pSmtMap->AddLayer(vl);
							m_pSmtMap->SetActiveOgrLayer(vl);
						}
					}
				}	

				leftover.Close();
			}
		}

		//////////////////////////////////////////////////////////////////////////

		infile.close();

		UpdateMapCatalog();
		Update2DXView();

		SmtListenerMsg param;
		param.hSrcWnd = NULL;
		post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMRESTORE,NULL),param);

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
			string strAppPath = get_app_path();
			char szParentPath[_MAX_DIR];
			char szBuf[_MAX_DIR];

			get_parent_directory(strAppPath.c_str(),szParentPath,1);

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
		m_pSmtMap->get_envelope(env);
		outfile.write((char*)(&env),sizeof(Envelope));*/
		
		int nLyrs = m_pSmtMap->GetLayerCount();
		outfile.write((char*)(&nLyrs),sizeof(int));

		SmtDataSource leftover;
		SmtDataSourceInfo info;
		char szLayerName[MAX_LAYER_NAME];  

		for (int i = 0; i < nLyrs; ++i)
		{
			leftover = SmtDataSource();
			if (OGRLayer *ogr = m_pSmtMap->GetOgrLayer(i))
			{
				leftover = SmtDataSource(ogr->GetDataset());
				strcpy_s(szLayerName,MAX_LAYER_NAME,ogr->GetName());
			}
			else if (SmtLayer *pLayer = m_pSmtMap->GetLeftoverLayer(i))
			{
				leftover = SmtDataSource(pLayer->GetDataSource());
				strcpy_s(szLayerName,MAX_LAYER_NAME,pLayer->GetLayerName());
			}

			leftover.GetInfo(info);
			outfile.write((char*)(&info),sizeof(SmtDataSourceInfo));
			outfile.write((char*)(szLayerName),sizeof(char)*MAX_LAYER_NAME);
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

	void SmtMapMgr::Set2DXViewNotify(Smt2DXViewNotifyFn fn)
	{
		m_fn2DXViewNotify = fn;
	}

	bool SmtMapMgr::Update2DXView(void)
	{
		if (m_fn2DXViewNotify == NULL)
			return true;

		vector<void*>::iterator iter = m_v2DXViewPtrs.begin();
		while(iter!=m_v2DXViewPtrs.end())
		{
			m_fn2DXViewNotify(*iter, m_pSmtMap);
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

	bool SmtMapMgr::AppendLayer(OGRLayer *pLayer)
	{
		bool bRet = true;

		if (m_pSmtMap)
		{
			bRet = m_pSmtMap->AddLayer(pLayer);
			m_pSmtMap->SetActiveOgrLayer(pLayer);

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