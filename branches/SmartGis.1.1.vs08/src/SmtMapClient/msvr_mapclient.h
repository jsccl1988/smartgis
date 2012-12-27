#ifndef _MSVR_MAPCLINET_H
#define _MSVR_MAPCLINET_H

#include "msvr_mapservice.h"
#include "gis_map.h"

using namespace Smt_MapService;
using namespace Smt_GIS;
using namespace Smt_Core;

namespace Smt_MapClient
{
	//client part
	class SMT_EXPORT_CLASS SmtTileWrapper 
	{
	public:
		SmtTileWrapper();
		virtual~SmtTileWrapper();

	public:
		long						Init(SmtMapService *pBindService,SmtTileLayer *pBindTileLayer);
		long						Create(void);
		long						Destroy(void);

	protected:
		SmtTileWrapper(const SmtTileWrapper& other) {;}
		SmtTileWrapper &			operator=(const SmtTileWrapper& other) {return *this;}

	public:
		//��ȡ��ǰ�󶨷���
		inline SmtMapService		*GetBindService(void) {return m_pBindService;}
		inline const SmtMapService  *GetBindService(void) const {return m_pBindService;}

		//��ȡ��ǰͬ��titlelayer
		inline SmtTileLayer			*GetBindTileLayer(void) {return m_pBindTileLayer;}
		inline const SmtTileLayer	*GetBindTileLayer(void) const {return m_pBindTileLayer;}

		//���õ�ǰ�ӿڴ�С
		inline long					SetClientRect(const lRect &lClientRect);

		//��ȡ��ǰ�ӿھ�γ�ȷ�Χ
		void						GetClientLBRect(fRect &rct);

		inline long					SetZoomCenter(const fPoint &center);
		inline long					GetZoomCenter(fPoint &center) const;

		inline long					SetZoom(long lZoom,const fPoint &orgPoint);
		inline long					SetZoom(long lZoom);
		inline long					GetZoom(void) const ;

		long						GetTileRange(long &lMinCol,long &lMinRow,long &lMaxCol,long &lMaxRow) const;

		long						Sync2TileLayer(void);

	protected:
		lRect						m_lClientRect;

		long						m_lZoom;
		fPoint						m_center;

		long						m_lTileWidth;
		long						m_lTileHeight;
		long						m_lTileImageCode;

		SmtMapService				*m_pBindService;
		SmtTileLayer				*m_pBindTileLayer;
	};

	class SMT_EXPORT_CLASS SmtWSMap:public SmtMap
	{//��ϵ�ͼ�ĵ�(֧��titlelayer)
	public:
		SmtWSMap(void);
		virtual ~SmtWSMap(void);

	public:
		bool                        AddTileLayer(const SmtLayer * pLayer,const SmtMapService *pBindService);

		bool                        RemoveTileLayer(const char * szName);
		bool                        RemoveTileLayer(const SmtTileLayer *pLayer);

		bool						QueryFeature(const SmtGQueryDesc *  pGQueryDesc,const SmtPQueryDesc * pPQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType);

		SmtTileWrapper*				GetTileWrapper(const char * szName); 

	public:
		//���õ�ǰ�ӿڴ�С
		inline long					SetClientRect(const lRect &lClientRect);

		//��ȡ��ǰ�ӿھ�γ�ȷ�Χ
		void						GetClientLBRect(fRect &rct);

		inline long					SetZoomCenter(const fPoint &center);
		inline long					GetZoomCenter(fPoint &center) const;

		inline long					SetZoom(long lZoom,const fPoint &orgPoint);
		inline long					SetZoom(long lZoom);
		inline long					GetZoom(void) const ;

		long						Sync2TileLayers(void);

	protected:
		lRect						m_lClientRect;

		long						m_lZoom;
		fPoint						m_center;

		vector<SmtTileWrapper*>		m_vTileWrappers;
	};
}

#if !defined(Export_SmtMapClient)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapClientD.lib")
#       else
#          pragma comment(lib,"SmtMapClient.lib")
#	    endif
#endif

#endif //_MSVR_MAPCLINET_H