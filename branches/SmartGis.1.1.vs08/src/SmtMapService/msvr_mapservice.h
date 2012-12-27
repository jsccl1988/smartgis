#ifndef _MSVR_MAPSERVICE_H
#define _MSVR_MAPSERVICE_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "bl_envelope.h"
#include "gis_map.h"
#include "rd_renderer.h"
#include "rd_renderdevice.h"
#include "smt_thread.h"
#include "smt_cslock.h"

//#define		IMAGE_CODE_TYPE					CXIMAGE_FORMAT_PNG

//0000 00000000000000 00000000000000
//zoom col			  row
//目前最多支持		  14级

#define		TITLE_GETUNIID(lZoom,lCol,lRow)	((lZoom)<<28|(lCol)<<14|(lRow))
#define		TITLE_GETZOOM(lUID)				((lUID)>>28)
#define		TITLE_GETCOL(lUID)				(((lUID)<<4 )>>18)
#define		TITLE_GETROW(lUID)				(((lUID)<<18)>>18)

namespace Smt_MapService
{
	//service part
	class SmtMapService;

	class SMT_EXPORT_CLASS SmtTileCache
	{
	public:
		SmtTileCache():m_strCacheDir("")
			,m_bOpen(false)
			,m_pMSvr(NULL)
		{
			;
		}

		virtual ~SmtTileCache() {}

	public:
		virtual long				Init(void) = 0;
		virtual long				Create(void) = 0;
		virtual long				Destroy(void) = 0;

		virtual long				Connect(std::string strCacheFileName) = 0;

		virtual	bool				Open(void) = 0;
		virtual	bool				Close(void) = 0;
		bool						IsOpen(void){return m_bOpen;};
		
	protected:
		SmtTileCache(const SmtTileCache& other) {;}
		SmtTileCache &				operator=(const SmtTileCache& other) {return *this;}

	public:
		inline void					SetMapService(SmtMapService *pMSvr) {m_pMSvr = pMSvr;}
		const SmtMapService *		GetMapService(void) const {return m_pMSvr;}
		SmtMapService *				GetMapService(void){return m_pMSvr;}

		inline void					SetCacheDir(std::string strCacheDir) {m_strCacheDir = strCacheDir;}
		std::string					GetCacheDir(void) const {return m_strCacheDir;}

	public:
		virtual bool				IsTileExist(long lCol,long lRow,long lZoom)  = 0;

		virtual long				AppendTile(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom) = 0;
		virtual long				GetTile(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom) = 0;
		virtual long				ReleaseTile(char *&pTileBuf) = 0;
		
	protected:
		std::string					m_strCacheDir;
		bool						m_bOpen;
	
		SmtMapService				*m_pMSvr;
	};

	class SMT_EXPORT_CLASS SmtMapService 
	{
	public:
		SmtMapService();
		virtual~SmtMapService();

	public:
		long						Init(const char * szLogname = "MSVR_MAPSERVICE");

		long						Create(void);

		long						Destroy(void);
		
	protected:
		SmtMapService(const SmtMapService& other) {;}
		SmtMapService &				operator=(const SmtMapService& other) {return *this;}

	public:
		inline void					SetName(std::string strSvrName) {m_strSvrName = strSvrName;}
		inline std::string			GetName(void) const {return m_strSvrName;}

		inline void					SetProvider(std::string strSvrProvider) {m_strSvrProvider = strSvrProvider;}
		inline std::string			GetProvider(void) const{return m_strSvrProvider;}

		inline void					SetTileCacheDir(std::string strTileCacheDir) {if (NULL != m_pTileCache)m_pTileCache->SetCacheDir(strTileCacheDir);}
		std::string					GetTileCacheDir(void) const {if (NULL != m_pTileCache)return m_pTileCache->GetCacheDir();return "";}

	public:
		inline	bool				IsCreate() {return m_bCreate;}
		inline	void				SetMapDoc(string strMDOC);			
		inline	string				GetMapDoc() const ;

		inline	void				SetZoomMax(long	lMaxZoom);			//0-13
		inline	long				GetZoomMax() const ;

		inline	void				SetZoomMin(long	lMinZoom);			//0-13
		inline	long				GetZoomMin() const ;

		//获取图像编码类型
		inline long					GetImageCode() const;
		inline void					SetImageCode(long lImageCode) ;

		//透明
		inline bool					IsTransparent() const;
		inline void					SetTransparent(bool bTransparent) ;

		inline  void				SetTileSize(long lWidth,long lHeight) {m_lTileWidth = lWidth ; m_lTileHeight = lHeight;}
		inline  void				GetTileSize(long &lWidth,long &lHeight) const{lWidth = m_lTileWidth;lHeight = m_lTileHeight;}

		//设置请求图层列表
		long						SetRequestLayers(const vector<string> &vStrReqLYRNames);
		long						GetRequestLayers(vector<string> &vStrReqLYRNames) const;

	public:
		long						CreateTileCache(void);
		long						ConnectTileCache(std::string strCacheFileName);

	public:
		inline	bool				IsShowVersion(void) const {return m_bShowVersion;}
		inline	void				ShowVersion(bool bShow = true) {m_bShowVersion = bShow;}

	public:
		//获取瓦片分辨率
		inline double				GetTileResolution(long lZoom) const;

		//获取像素分辨率
		inline double				GetXPixelResolution(long lZoom) const;
		inline double				GetYPixelResolution(long lZoom) const;

		//根据像素分辨率获取zoom
		long						GetZoomByPixelResolution(double dbfRes);
		long						GetNearestZoomByPixelResolution(double dbfRes);

		//当前Zoom 瓦片范围
		long						GetTileRange(long &lMinCol,long &lMinRow,long &lMaxCol,long &lMaxRow,const Smt_Base::Envelope &env,long lZoom) const;
		long						GetTileLongLatRange(Smt_Base::Envelope &llEnv,long lCol,long lRow,long lZoom) const;

		//静态瓦片图
		long						GetTileImage(char *&pTileImgBuf,long &lTileImgBufSize,long lCol,long lRow,  long lZoom);
		long						ReleaseTileImage(char *&pTileImgBuf);

		//实时矢量图
		long						GetVectorImage(char *&pVectorImgBuf,long &lVectorImgBufSize,const Smt_Core::fRect &rect, long lZoom,const vector<string> &vStrReqLYRNames);
		long						GetVectorImage(char *&pVectorImgBuf,long &lVectorImgBufSize,const Smt_Core::fRect &rect, const Smt_Core::lRect &view,const vector<string> &vStrReqLYRNames);
		long						ReleaseVectorImage(char *&pVectorImgBuf);

		void						GetEnvelope(Smt_Base::Envelope &env) const;

		//矢量查询
		bool						QueryFeature(const SmtGQueryDesc *  pGQueryDesc,const SmtPQueryDesc * pPQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType);

	public:
		Smt_GIS::SmtMap *			GetSmtMapPtr(void);
		const Smt_GIS::SmtMap *		GetSmtMapPtr(void) const;

		Smt_GIS::SmtMap&			GetSmtMap(void);
		const Smt_GIS::SmtMap&		GetSmtMap(void) const;

	protected:
		static bool					InitRenderWindow(void);
		static bool					InitRender(void);

	protected:
		long						CreateZoomTiles(long lZoom);

		long						CreateZoomTiles_Split(long lZoom,long lMinCol,long lMinRow,long lMaxCol,long lMaxRow);

	protected:
		static HWND					m_hImageWnd;
		static Smt_Rd::LPRENDERER     m_pRenderer;
		static Smt_Rd::LPRENDERDEVICE m_pRenderDevice;

	protected:
		std::string					m_strSvrName;
		std::string					m_strSvrProvider;
	
		long						m_lZoomMax;
		long						m_lZoomMin;

		long						m_lTileWidth;
		long						m_lTileHeight;
		long						m_lImageCode;
		bool						m_bTransparent;

		bool						m_bShowVersion;

		Smt_GIS::SmtMap				m_smtMap;
		string						m_strMDOC;
		vector<Smt_GIS::SmtLayer*>	m_vRequestLayers;

		ulong						m_ulTileNum;
		std::string					m_strLog;
		SmtTileCache				*m_pTileCache;

		Smt_Core::SmtCSLock			m_cslock;

		bool						m_bCreate;
	};
}

#if !defined(Export_SmtMapService)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServiceD.lib")
#       else
#          pragma comment(lib,"SmtMapService.lib")
#	    endif
#endif

#endif //_MSVR_MAPSERVICE_H