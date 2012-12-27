#include "msvr_mapservice.h"
#include "sys_sysmanager.h"
#include "smt_logmanager.h"
#include "bl_api.h"
#include "smt_api.h"
#include "ximage.h"
#include "msvr_dirtilecache.h"
#include "msvr_smftilecache.h"
#include "cata_mapmgr.h"

#include <iostream>
using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_GIS;
using namespace Smt_Rd;
using namespace Smt_Sys;
using namespace Smt_Base;
using namespace Smt_XCatalog;

namespace Smt_MapService
{
	HWND			SmtMapService::m_hImageWnd = NULL;
	LPRENDERER		SmtMapService::m_pRenderer = NULL;
	LPRENDERDEVICE	SmtMapService::m_pRenderDevice = NULL;

	LRESULT CALLBACK WndProc(HWND hWnd,
		UINT	message,
		WPARAM	wParam,
		LPARAM	lParam)
	{
		return(1L);
	}

	//long range:-180~180
	//lat  range:-90~90
	static double gs_TileResolution[] = {90,30,15,10,4,2,1,0.4,0.2,0.1,0.004,0.002,0.001,0.0004};
	//////////////////////////////////////////////////////////////////////////
	//SmtMapService
	//////////////////////////////////////////////////////////////////////////
	SmtMapService::SmtMapService():m_strSvrName("")
		,m_strSvrProvider("")
		,m_lZoomMax(4)
		,m_lZoomMin(0)
		,m_lTileWidth(256)
		,m_lTileHeight(256)
		,m_pTileCache(NULL)
		,m_bShowVersion(true)
		,m_ulTileNum(0)
		,m_lImageCode(CXIMAGE_FORMAT_PNG)
		,m_bTransparent(false)
		,m_bCreate(false)
	{

	}

	SmtMapService::~SmtMapService()
	{
		Destroy();
	}

	long SmtMapService::Init(const char * szLogname)
	{
		if (m_hImageWnd == NULL ||
			m_pRenderer  == NULL || m_pRenderDevice == NULL)
		{
			if (!InitRenderWindow())
				return SMT_ERR_FAILURE;

			if (!InitRender())
				return SMT_ERR_FAILURE;
		}

		if (strlen(szLogname) == 0)
			m_strLog = "MSVR_MAPSERVICE";
		else
			m_strLog = szLogname;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(m_strLog.c_str());
		if (NULL == pLog)
		{
			pLog = pLogMgr->CreateLog(m_strLog.c_str());
		}
		
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		string strAppPath = GetAppPath();
		string strCacheDir = strAppPath + "msvr\\tilecache\\" + m_strSvrName+"\\";

		CreateAllPathDirectory(strCacheDir);

		//m_pTileCache = new SmtDirTileCache();
		m_pTileCache = new SmtSmfTileCache();
		m_pTileCache->SetMapService(this);
		m_pTileCache->SetCacheDir(strCacheDir);
		
		if( SMT_ERR_NONE == m_pTileCache->Init() )
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::Create(void)
	{
		if (m_bCreate)
		{
			if (SMT_ERR_NONE != Destroy())
				return SMT_ERR_FAILURE;
		}
		 
		if (m_pTileCache->Open() &&
			NULL != m_pRenderDevice && 
			SmtMapMgr::OpenMap(&m_smtMap,m_strMDOC.c_str()))
		{
			m_bCreate = true;
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::CreateTileCache(void)
	{
		if (SMT_ERR_NONE == m_pTileCache->Create())
		{
			m_pTileCache->Open();

			m_ulTileNum = 0;
			for (int i = m_lZoomMin; i <= m_lZoomMax;i++)
				CreateZoomTiles(i);

			m_pTileCache->Close();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::ConnectTileCache(std::string strCacheFileName)
	{
		if (SMT_ERR_NONE == m_pTileCache->Connect(strCacheFileName) &&
			m_pTileCache->Open() )
		{
			m_pTileCache->Close();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::Destroy()
	{
		if (NULL != m_pTileCache)
		{
			m_bCreate = false;

			if (SMT_ERR_NONE ==m_pTileCache->Destroy())
			{
				SMT_SAFE_DELETE(m_pTileCache);
				return SMT_ERR_NONE;
			}
			else
			{
				SMT_SAFE_DELETE(m_pTileCache);
				return SMT_ERR_FAILURE;
			}
		}

		m_vRequestLayers.clear();

		return SMT_ERR_NONE;
	}

	long SmtMapService::CreateZoomTiles(long lZoom)
	{
		long lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope env;

		//计算title范围
		m_smtMap.GetEnvelope(env);

		GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow,env,lZoom);

		m_ulTileNum = 0;
		CreateZoomTiles_Split(lZoom,lMinCol,lMinRow,lMaxCol,lMaxRow);

		return SMT_ERR_NONE;
	}

	long SmtMapService::CreateZoomTiles_Split(long lZoom,long lMinCol,long lMinRow,long lMaxCol,long lMaxRow)
	{
		if ( (lMaxCol - lMinCol + 1)*m_lTileWidth > 8192 || (lMaxRow - lMinRow +1)*m_lTileHeight > 8192)
		{//继续划分
			CreateZoomTiles_Split(lZoom,lMinCol,lMinRow,(lMinCol+lMaxCol)/2,(lMinRow+lMaxRow)/2);
			CreateZoomTiles_Split(lZoom,(lMinCol+lMaxCol)/2+1,lMinRow,lMaxCol,(lMinRow+lMaxRow)/2);
			CreateZoomTiles_Split(lZoom,(lMinCol+lMaxCol)/2+1,(lMinRow+lMaxRow)/2+1,lMaxCol,lMaxRow);
			CreateZoomTiles_Split(lZoom,lMinCol,(lMinRow+lMaxRow)/2+1,(lMinCol+lMaxCol)/2,lMaxRow);
		}
		else
		{
			if (lMaxCol < lMinCol || lMaxRow < lMinRow)
			{
				return SMT_ERR_NONE;
			}
			//绘制
			Envelope llEnv;
			Envelope titleEnv;
			for (int i = lMinCol; i <= lMaxCol;i++)
			{
				for(int j = lMinRow;j <= lMaxRow;j++)
				{
					GetTileLongLatRange(titleEnv,i,j,lZoom);

					llEnv.Merge(titleEnv);
				}
			}

			fRect	 rect;
			EnvelopeToRect(rect,llEnv);

			m_pRenderDevice->Resize(0,0,(lMaxCol - lMinCol + 1)*m_lTileWidth,(lMaxRow - lMinRow +1)*m_lTileHeight);

			m_pRenderDevice->ZoomToRect(&m_smtMap,rect);

			m_pRenderDevice->BeginRender(MRD_BL_MAP,true);

			m_pRenderDevice->RenderMap(&m_smtMap);

			m_pRenderDevice->EndRender(MRD_BL_MAP);

			//分割
			char *pImageBuf = NULL;
			long lImageBufSize = 0;

			m_pRenderDevice->Save2ImageBuf(pImageBuf,lImageBufSize,m_lImageCode,MRD_BL_MAP);

			CxImage largeImage((BYTE*)pImageBuf,lImageBufSize,m_lImageCode);

			CxImage	cropImage;
			RECT	cropRect;

			BYTE	*pTileBuf = NULL;
			long	lTileBufSize = 0;

			for (int i = lMinCol; i <= lMaxCol;i++)
			{
				cropRect.left = (i-lMinCol)*m_lTileWidth;
				cropRect.right = cropRect.left + m_lTileWidth;

				for(int j = lMinRow;j <= lMaxRow;j++)
				{
					cropRect.top = (lMaxRow - j)*m_lTileHeight;
					cropRect.bottom =cropRect.top + m_lTileHeight;

					largeImage.Crop(cropRect,&cropImage);
					cropImage.Encode(pTileBuf,lTileBufSize,m_lImageCode);
					m_pTileCache->AppendTile((const char *)pTileBuf,lTileBufSize,i,j,lZoom);
					cropImage.FreeMemory(pTileBuf);
					pTileBuf = NULL;

					m_ulTileNum ++;
				}
			}

			m_pRenderDevice->FreeImageBuf(pImageBuf);

			SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
			SmtLog *pLog = pLogMgr->GetLog(m_strLog.c_str());
			if (pLog != NULL)
				pLog->LogMessage("%s Zoom:%d\tTile Num = %d\t(%d,%d)--(%d,%d)",m_strSvrName.c_str(),lZoom,m_ulTileNum,lMinCol,lMinRow,lMaxCol,lMaxRow);

		}

		return SMT_ERR_NONE;
	}

	void SmtMapService::GetEnvelope(Envelope &env) const
	{
		m_smtMap.GetEnvelope(env);
	}

	//////////////////////////////////////////////////////////////////////////
	inline	void SmtMapService::SetMapDoc(string strMDOC)
	{
		m_strMDOC = strMDOC;
	}

	inline	string SmtMapService::GetMapDoc() const 
	{
		return m_strMDOC;
	}

	inline long SmtMapService::GetImageCode() const
	{
		return m_lImageCode;
	}

	inline void SmtMapService::SetImageCode(long lImageCode)
	{
		m_lImageCode = lImageCode;
	}

	inline bool SmtMapService::IsTransparent() const
	{
		return m_bTransparent;
	}
	inline void SmtMapService::SetTransparent(bool bTransparent)
	{
		m_bTransparent = bTransparent;
	}

	inline	void SmtMapService::SetZoomMax(long	lMaxZoom)
	{
		int _lMaxZoom = sizeof(gs_TileResolution)/sizeof(double);

		m_lZoomMax = (lMaxZoom < _lMaxZoom)?lMaxZoom:_lMaxZoom;
	}			

	inline	long SmtMapService::GetZoomMax() const 
	{
		return m_lZoomMax;
	}

	inline	void SmtMapService::SetZoomMin(long	lMinZoom)
	{
		m_lZoomMin = (lMinZoom < 0)?0:lMinZoom;
	}			

	inline	long SmtMapService::GetZoomMin() const 
	{
		return m_lZoomMin;
	}

	inline double SmtMapService::GetTileResolution(long lZoom) const
	{
		if (lZoom < m_lZoomMin || lZoom >= sizeof(gs_TileResolution)/sizeof(double))
			return -1;
		 
		return gs_TileResolution[lZoom];
	}

	inline double SmtMapService::GetXPixelResolution(long lZoom) const
	{
		if (lZoom < m_lZoomMin || lZoom >= sizeof(gs_TileResolution)/sizeof(double))
			return -1;

		return gs_TileResolution[lZoom]/m_lTileWidth;
	}

	inline double SmtMapService::GetYPixelResolution(long lZoom) const
	{
		if (lZoom < m_lZoomMin || lZoom >= sizeof(gs_TileResolution)/sizeof(double))
			return -1;

		return gs_TileResolution[lZoom]/m_lTileHeight;
	}

	inline	long SmtMapService::GetZoomByPixelResolution(double dbfRes)
	{
		int nZoom = sizeof(gs_TileResolution)/sizeof(double);
		for (int iZoom = 0; iZoom < nZoom;iZoom++)
		{
			if (IsEqual(dbfRes,GetXPixelResolution(iZoom),dEPSILON))
			{
				return iZoom;
			}
		}

		return -1;
	}

	inline	long SmtMapService::GetNearestZoomByPixelResolution(double dbfRes)
	{
		int nZoom = sizeof(gs_TileResolution)/sizeof(double);
		int iZoom = 0;

		while (iZoom < nZoom && 
			GetXPixelResolution(iZoom) > dbfRes)
		{
			iZoom++;
		}

		if (iZoom == 0)
		{
			;
		}
		else if (iZoom == nZoom-1)
		{
			iZoom = nZoom-1;
		}
		else
		{
			if (2*dbfRes -GetXPixelResolution(iZoom) - GetXPixelResolution(iZoom-1) < 0)
				iZoom--;
		}

		return iZoom;
	}

	long SmtMapService::GetTileRange(long &lMinCol,long &lMinRow,long &lMaxCol,long &lMaxRow,const Envelope &env,long lZoom) const
	{
		if (lZoom < m_lZoomMin || lZoom > m_lZoomMax)
			return SMT_ERR_FAILURE;
		
		lMinCol = (env.MinX+180.)/gs_TileResolution[lZoom];
		lMinRow = (env.MinY+90.)/gs_TileResolution[lZoom];

		lMaxCol = (env.MaxX+180.)/gs_TileResolution[lZoom];
		lMaxRow = (env.MaxY+90.)/gs_TileResolution[lZoom];
		 
		return SMT_ERR_NONE;
	}

	long SmtMapService::GetTileLongLatRange(Envelope &llEnv,long lCol,long lRow,long lZoom) const
	{	
		if (lZoom < m_lZoomMin || lZoom > m_lZoomMax)
			return SMT_ERR_FAILURE;

		llEnv.MinX = lCol*gs_TileResolution[lZoom]-180.;
		llEnv.MinY = lRow*gs_TileResolution[lZoom]-90.;

		llEnv.MaxX = (lCol+1)*gs_TileResolution[lZoom]-180.;
		llEnv.MaxY = (lRow+1)*gs_TileResolution[lZoom]-90.;

		return SMT_ERR_NONE;
	}

	long SmtMapService::GetTileImage(char *&pTileImgBuf,long &lTileImgBufSize,long lCol,long lRow,long lZoom)
	{
		if (NULL == m_pTileCache)
			return SMT_ERR_FAILURE;
		
		m_pTileCache->GetTile(pTileImgBuf,lTileImgBufSize,lCol,lRow,lZoom);

		return SMT_ERR_NONE;
	}

	long SmtMapService::ReleaseTileImage(char *&pTileImgBuf)
	{
		if (NULL != m_pTileCache)
			return m_pTileCache->ReleaseTile(pTileImgBuf);

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::GetVectorImage(char *&pVectorImgBuf,long &lVectorImgBufSize,const fRect &rect, long lZoom,const vector<string> &vStrReqLYRNames)
	{
		if (NULL != m_pRenderDevice)
		{
			m_cslock.Lock();

			if (SMT_ERR_NONE != SetRequestLayers(vStrReqLYRNames))
			{//要考虑服务器并发，多线程进行操作时必须保证，成员变量在竞争区（CS锁）内改变
			 //遇到一个bug即请求图层在外面改变时，因为线程的并发导致不一致，出现错误
				m_cslock.Unlock();
				return SMT_ERR_FAILURE;
			}

			m_pRenderDevice->Lock();

			//绘制
			m_pRenderDevice->Resize(0,0,rect.Width()/GetXPixelResolution(lZoom),rect.Height()/GetYPixelResolution(lZoom));

			m_pRenderDevice->ZoomToRect(&m_smtMap,rect);

			m_pRenderDevice->BeginRender(MRD_BL_MAP,true);

			for (int i = 0; i < m_vRequestLayers.size();i++)
				m_pRenderDevice->RenderLayer(m_vRequestLayers[i]);

			m_pRenderDevice->EndRender(MRD_BL_MAP);

			m_pRenderDevice->Save2ImageBuf(pVectorImgBuf,lVectorImgBufSize,m_lImageCode,MRD_BL_MAP,m_bTransparent);

			m_pRenderDevice->Unlock();

			m_cslock.Unlock();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::GetVectorImage(char *&pVectorImgBuf,long &lVectorImgBufSize,const fRect &rect,const Smt_Core::lRect &view,const vector<string> &vStrReqLYRNames)
	{
		if (NULL != m_pRenderDevice)
		{
			m_cslock.Lock();

			if (SMT_ERR_NONE != SetRequestLayers(vStrReqLYRNames))
			{//要考虑服务器并发，多线程进行操作时必须保证，成员变量在竞争区（CS锁）内改变
			 //遇到一个bug即请求图层在外面改变时，因为线程的并发导致不一致，出现错误
				m_cslock.Unlock();
				return SMT_ERR_FAILURE;
			}

			m_pRenderDevice->Lock();

			//绘制
			m_pRenderDevice->Resize(0,0,view.Width(),view.Height());

			m_pRenderDevice->ZoomToRect(&m_smtMap,rect);

			m_pRenderDevice->BeginRender(MRD_BL_MAP,true);

			for (int i = 0; i < m_vRequestLayers.size();i++)
				m_pRenderDevice->RenderLayer(m_vRequestLayers[i]);
			
			m_pRenderDevice->EndRender(MRD_BL_MAP);

			m_pRenderDevice->Save2ImageBuf(pVectorImgBuf,lVectorImgBufSize,m_lImageCode,MRD_BL_MAP,m_bTransparent);

			m_pRenderDevice->Unlock();
			
			m_cslock.Unlock();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapService::ReleaseVectorImage(char *&pVectorImgBuf)
	{
		SMT_SAFE_DELETE_A(pVectorImgBuf);

		return SMT_ERR_NONE;
	}

	bool SmtMapService::QueryFeature(const SmtGQueryDesc *  pC_GQueryDesc,const SmtPQueryDesc * pC_PQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType)
	{
		return m_smtMap.QueryFeature(pC_GQueryDesc,pC_PQueryDesc,pQueryResult,nFeaType);
	}

	//////////////////////////////////////////////////////////////////////////
	//地图服务管理
	SmtMap *SmtMapService::GetSmtMapPtr(void)
	{
		return &m_smtMap;
	}

	const SmtMap * SmtMapService::GetSmtMapPtr(void) const
	{
		return &m_smtMap;
	}

	SmtMap& SmtMapService::GetSmtMap(void)
	{
		return m_smtMap;
	}

	const SmtMap& SmtMapService::GetSmtMap(void) const
	{
		return m_smtMap;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapService::InitRenderWindow()
	{
		WNDCLASS wc;
		DWORD dwExStyle;
		DWORD dwStyle;
		RECT WindowRect;
		HMODULE hInstance = NULL;
		HWND hWnd = NULL;
		WindowRect.left=(long)0;
		WindowRect.right=(long)0;
		WindowRect.top=(long)0;
		WindowRect.bottom=(long)0;
		hInstance = GetModuleHandle(NULL);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.lpfnWndProc = (WNDPROC) WndProc;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = "IMAGEWINDOW";

		if (!RegisterClass(&wc))
		{
			return false;
		}

		dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle=WS_OVERLAPPEDWINDOW;

		AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);

		if (!(m_hImageWnd=CreateWindowEx( dwExStyle,
			"IMAGEWINDOW",
			"temp",
			dwStyle |
			WS_CLIPSIBLINGS |
			WS_CLIPCHILDREN,
			0, 0,
			WindowRect.right-WindowRect.left,
			WindowRect.bottom-WindowRect.top,
			NULL,
			NULL,
			hInstance,
			NULL)))
		{
			return false;
		}

		return true;
	}

	bool SmtMapService::InitRender(void)
	{
		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();

		SmtLog *pLog = pLogMgr->GetDefaultLog();
		SmtSysPra sysPra = pSysMgr->GetSysPra();

		string strMapRenderDevice = /*sysPra.str2DRenderDeviceName*/"SmtGdiSimpleRenderDevice";
		m_pRenderer = new SmtRenderer(GetModuleHandle(NULL));
		if (m_pRenderer->CreateDevice(strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
			return false;

		m_pRenderDevice = m_pRenderer->GetDevice();
		if (m_pRenderDevice == NULL)
			return false;

		if (m_pRenderDevice->Init(m_hImageWnd,strMapRenderDevice.c_str()) == SMT_ERR_FAILURE)
		{
			pLog->LogMessage("Init %s failure!",strMapRenderDevice.c_str());
			return false;
		}
		
		Smt2DRenderPra	rdPra;

		rdPra.bShowMBR		= false;
		rdPra.bShowPoint	= false;
		rdPra.lPointRaduis	= 4;

		m_pRenderDevice->SetMapMode(MM_TEXT);
		m_pRenderDevice->SetRenderPra(rdPra);

		//pLog->LogMessage("Init %s ok!",strMapRenderDevice.c_str());

		return true;
	}

	long SmtMapService::SetRequestLayers(const vector<string> &vStrReqLYRNames)
	{
		if (vStrReqLYRNames.size() < 1)
			return SMT_ERR_NONE;

		m_vRequestLayers.clear();

		SmtLayer* pLayer = NULL;
		for (int i = 0; i < vStrReqLYRNames.size();i++)
		{
			if ( (pLayer = m_smtMap.GetLayer(vStrReqLYRNames[i].c_str())) != NULL)
			{
				m_vRequestLayers.push_back(pLayer);
			}
		}

		if (m_vRequestLayers.size() < 1)
			return SMT_ERR_NONE;
		 
		return SMT_ERR_NONE;
	}

	long SmtMapService::GetRequestLayers(vector<string> &vStrReqLYRNames) const
	{
		for (int i = 0; i < m_vRequestLayers.size();i++)
			vStrReqLYRNames.push_back(m_vRequestLayers[i]->GetLayerName());

		return SMT_ERR_NONE;
	}	

}