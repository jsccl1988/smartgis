/*
File:    gis_sde.h

Desc:    SmtDataSource,SmtVectorLayer SmtRasterLayer SmtTileLayer 采用数据源+图层的结构

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_SDE_H
#define _GIS_SDE_H

#include "gis_feature.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_Base;

#define  MAX_DS_NAME                 MAX_NAME_LENGTH
#define  MAX_SVR_NAME                MAX_FILE_PATH
#define  MAX_DB_NAME                 MAX_NAME_LENGTH
#define  MAX_FILE_NAME               MAX_NAME_LENGTH
#define  MAX_UID_NAME                MAX_NAME_LENGTH
#define  MAX_PWD_NAME                MAX_NAME_LENGTH
#define  MAX_SPATIALINDEX_NAME		 MAX_NAME_LENGTH
#define  MAX_LAYER_NAME              MAX_NAME_LENGTH
#define  MAX_URL_LENGTH              MAX_FILE_PATH    
#define  MAX_LAYER_ARCHIVE_NAME      (MAX_FILE_PATH + MAX_NAME_LENGTH)
#define  MAX_LAYER_SRS_NAME			 MAX_NAME_LENGTH

namespace Smt_GIS
{
	enum eSmtDBProvider
	{
		PROVIDER_ACCESS,			//access 数据库
		PROVIDER_SQLSERVER,			//sql server 数据库
		PROVIDER_ORACLE,			//Oracle 数据库
		PROVIDER_MYSQL,				//MySQL 数据库
	};

	enum eSmtFileProvider
	{
		PROVIDER_SHAPE,				//shape文件
		PROVIDER_OGR_SUPPORT		//gdal ogr 支持的文件
	};

	enum eSmtWSProvider
	{
		PROVIDER_SMARTGIS,			//SMARTGIS 地图服务
	};

	enum eSmtMemProvider
	{
		PROVIDER_MEM_VER1			//内存，版本1
	};

	enum eDSType
	{
		DS_FILE_SMF,
		DS_DB_ADO,
		DS_DB_ODBC,
		DS_DB_MYSQL,
		DS_DB_ORACLE,
		DS_MEM,
		DS_WS,						//web数据源
	};

	//////////////////////////////////////////////////////////////////////////
	struct SmtGQueryDesc
	{//查询 几何条件
		SmtGeometry *pQueryGeom;	//几何条件
		SmtSpatialRs sSRs;			//查询关系
		float        fSmargin;		//容差

		SmtGQueryDesc(void)
		{
			pQueryGeom = NULL;
			sSRs       = SS_Contains;
			fSmargin   = 0.05;
		}
	};

	struct SmtPQueryDesc
	{//查询 属性条件
		char **szFldName;			// color,area
		char **szFldQueryContent;	// = RGB(255,255,255), > 100.

		SmtPQueryDesc(void)
		{
			szFldName = NULL;
			szFldQueryContent = NULL;
		}
	};

	struct SmtSpIdxInfo
	{
		char szName[MAX_SPATIALINDEX_NAME];
		uint unType;
		char szOnwerLayerName[MAX_LAYER_NAME];

		SmtSpIdxInfo()
		{
			szName[0] = '\0';
			unType = 0;
			szOnwerLayerName[0] = '\0';
		}
	};

	struct SmtLayerInfo 
	{// layer 信息
		char szName[MAX_LAYER_NAME];				//图层名称
		char szArchiveName[MAX_LAYER_ARCHIVE_NAME];	//物理存档信息
		char szSRS[MAX_LAYER_SRS_NAME];				//空间参照系名称			
		uint unFeatureType;							//图层类型
		uint unSIType;								//空间索引类型
		fRect lyrRect;

		SmtLayerInfo()
		{
			szName[0] = '\0';
			szArchiveName[0] = '\0';
			szSRS[0] = '\0';
			unFeatureType = 0;
			unSIType = 0;
		}
	};

	struct SmtDataSourceInfo
	{//数据源信息
		uint unProvider;
		uint unType;
		char szName[MAX_DS_NAME];
		char szUrl[MAX_URL_LENGTH];

		union
		{
			struct
			{
				char szService[MAX_SVR_NAME];
				char szDBName[MAX_DB_NAME];
			} db;

			struct
			{
				char szPath[MAX_FILE_PATH];
				char szFileName[MAX_FILE_NAME];
			} file;
		};

		char szUID[MAX_UID_NAME];
		char szPWD[MAX_PWD_NAME];

		SmtDataSourceInfo()
		{
			unType = 0;
			unProvider = 0;
			szName[0] = '\0';
			szUrl[0] = '\0';

			//
			db.szService[0] = '\0';
			db.szDBName[0] = '\0';

			szUID[0] = '\0';
			szPWD[0] = '\0';
		}
	};

	enum eSmtFetchType
	{// 获取方式
		FETCH_ALL,
		FETCH_FILTER
	};

	//////////////////////////////////////////////////////////////////////////

	class  SmtDataSource;
	class  SmtVectorLayer;

	class SmtSpatialIndex
	{
	public:
		SmtSpatialIndex():m_pSrcLayer(NULL)
		{ 

		}

		virtual ~SmtSpatialIndex()
		{
			m_pSrcLayer = NULL;
		}

	public:
		void					 SetName(const char *szName) { sprintf_s(m_szName,MAX_SPATIALINDEX_NAME,szName);}
		inline const char		*GetName(void) {return m_szName;}

		void					 SetType(uint type) { m_unType = type;}
		inline uint				 GetType(void) { return m_unType;}

	public:
		virtual long			 Create(SmtVectorLayer * pSrcLayer) = 0;
		virtual long			 Delete() = 0;

		virtual long			 Query(SmtGQueryDesc *  pGQueryDesc,SmtVectorLayer * &pQueryResult) = 0;

	protected:
		SmtVectorLayer			*m_pSrcLayer;
		char					 m_szName[MAX_SPATIALINDEX_NAME];
		uint					 m_unType;
	};

	typedef				vector<SmtSpatialIndex*>		vSpatialIndexPtrs;
	typedef				vector<SmtSpIdxInfo>			vSmtSpIdxInfos;

	enum SmtLayerType
	{
		LYR_VECTOR,		//矢量
		LYR_RASTER,		//栅格
		LYR_TITLE,		//瓦片--支持web
	};

	enum SmtFeatureType_Ext
	{
		SmtLayer_Ras = SmtFtUnknown+1,
		SmtLayer_Tile = SmtFtUnknown+2,
	};

	//////////////////////////////////////////////////////////////////////////
	class  SmtLayer
	{
	public:
		SmtLayer(SmtDataSource *pOwnerDs) 
		{
			m_pAtt = NULL;
			m_pOwnerDs = NULL;
			/*m_pAtt = new SmtAttribute()*/;
			m_bOpen = false;
		}

		virtual ~SmtLayer(void) 
		{
			//////////////////////////////////////////////////////////////////////////
			SMT_SAFE_DELETE(m_pAtt);
		}

	public:
		const SmtDataSource		*GetDataSource(void) const {return m_pOwnerDs;}

	public:
		//////////////////////////////////////////////////////////////////////////
		virtual bool             Create(void) = 0;
		virtual bool             Open(const char * szLayerArchiveName) = 0;
		virtual bool             Close(void) = 0;
		virtual bool             Fetch(eSmtFetchType type = FETCH_ALL) = 0;

		inline	bool			 IsOpen(void) const {return m_bOpen;}

		void                     SetAttribute(const SmtAttribute  *pAtt) 
		{
			SMT_SAFE_DELETE(m_pAtt);

			if( pAtt != NULL )
				m_pAtt = pAtt->Clone();
			else
				m_pAtt = NULL;
		}

		inline SmtAttribute		*GetAttribute(void){return m_pAtt;}
		inline const SmtAttribute	*GetAttribute(void) const {return m_pAtt;}

		void                     GetEnvelope(Envelope &env) const{ memcpy(&env,&m_lyrEnv,sizeof(Envelope));}
		virtual void             CalEnvelope(void){};

		//////////////////////////////////////////////////////////////////////////
		//transcation
		virtual long             StartTransaction() { return SMT_ERR_NONE; }
		virtual long             CommitTransaction() { return SMT_ERR_NONE; }
		virtual long             RollbackTransaction() {return SMT_ERR_NONE; }

		//////////////////////////////////////////////////////////////////////////
		//layer info
		void                     SetLayerName(const char *szName) { sprintf_s(m_szLayerName,MAX_LAYER_NAME,szName);}
		const char *             GetLayerName(void) const {return m_szLayerName;}

		void                     SetSRS(const char *szSRS) { sprintf_s(m_szSRS,MAX_LAYER_SRS_NAME,szSRS);}
		const char *             GetSRS(void) const {return m_szSRS;}

		virtual void			 SetLayerRect(const fRect &lyrRect) = 0;
		virtual SmtLayerType	 GetLayerType(void) const= 0;

		inline void              SetVisible(bool bVisible = true) { m_bIsVisible = bVisible; }
		inline bool              IsVisible(void) const{return m_bIsVisible;}

	protected:
		SmtDataSource            *m_pOwnerDs;
		SmtAttribute			 *m_pAtt;

		Envelope				 m_lyrEnv;

		char                     m_szLayerName[MAX_LAYER_NAME];  
		char                     m_szSRS[MAX_LAYER_SRS_NAME];  
		bool                     m_bIsVisible;
		bool					 m_bOpen;
	};

	class  SmtVectorLayer: public SmtLayer
	{
	public:
		SmtVectorLayer(SmtDataSource *pOwnerDs):SmtLayer(pOwnerDs)
		{
			m_pFilterGeom = NULL;
		}

		virtual ~SmtVectorLayer(void) 
		{
			//////////////////////////////////////////////////////////////////////////
			//删除空间索引
			vSpatialIndexPtrs::iterator i = m_vSpIdxPtrs.begin() ;
			while (i != m_vSpIdxPtrs.end())
			{
				SMT_SAFE_DELETE(*i);
				++i;
			}
			m_vSpIdxPtrs.clear();

			//////////////////////////////////////////////////////////////////////////
			SMT_SAFE_DELETE(m_pFilterGeom);
		}

	public:
		//////////////////////////////////////////////////////////////////////////
		virtual bool             Create(void) = 0;
		virtual bool             Open(const char * szLayerArchiveName) = 0;
		virtual bool             Close(void) = 0;
		virtual bool             Fetch(eSmtFetchType type = FETCH_ALL) = 0;

		//////////////////////////////////////////////////////////////////////////
		//filter
		void                     SetFilterGeometry(const SmtGeometry *pFilterGeom, bool bIsFilterByGeom = false)
		{
			m_bIsFilterByGeom = bIsFilterByGeom;
			m_pFilterGeom = pFilterGeom->Clone();
		}

		void                     SetRectFilter(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
		{
			SmtLinearRing  oRing;
			SmtPolygon oPoly;

			oRing.AddPoint( dfMinX, dfMinY );
			oRing.AddPoint( dfMinX, dfMaxY );
			oRing.AddPoint( dfMaxX, dfMaxY );
			oRing.AddPoint( dfMaxX, dfMinY );
			oRing.AddPoint( dfMinX, dfMinY );

			oPoly.AddRing( &oRing );

			SetFilterGeometry(&oRing);
		}
 
		//////////////////////////////////////////////////////////////////////////
		//record set oper
		virtual int              GetFeatureCount(void) const = 0;

		virtual void             MoveFirst(void) const= 0;
		virtual void             MoveNext(void) const= 0;
		virtual void             MoveLast(void) const= 0;
		virtual void             Delete(void) = 0;
		virtual bool             IsEnd(void) const= 0;

		virtual void			 DeleteAll(void) = 0;

		//////////////////////////////////////////////////////////////////////////
		//spatial index
		virtual long			 CreateSpatialIndex(const char *szName,uint type) = 0;

		long					 DeleteSpatialIndex(const char *szName)
		{
			vSpatialIndexPtrs::iterator iter = m_vSpIdxPtrs.begin() ;
			while (iter != m_vSpIdxPtrs.end())
			{
				if (strcmp(szName,(*iter)->GetName()) == 0)
				{
					SMT_SAFE_DELETE(*iter);
					break;
				}
				++iter;
			}

			m_vSpIdxPtrs.erase(iter);

			return SMT_ERR_NONE;
		}

		long					 GetSpatialIndexInfos(vSmtSpIdxInfos &vInfos)
		{
			SmtSpIdxInfo info;
			vSpatialIndexPtrs::iterator iter = m_vSpIdxPtrs.begin() ;
			while (iter != m_vSpIdxPtrs.end())
			{
				sprintf_s(info.szName,MAX_SPATIALINDEX_NAME,(*iter)->GetName());
				sprintf_s(info.szOnwerLayerName,MAX_LAYER_NAME,m_szLayerName);
				info.unType = (*iter)->GetType();
				vInfos.push_back(info);
				++iter;
			}
		}

		long					 SetActiveSpatialIndex(const char *szName)
		{
			vSpatialIndexPtrs::iterator iter = m_vSpIdxPtrs.begin() ;
			while (iter != m_vSpIdxPtrs.end())
			{
				if (strcmp(szName,(*iter)->GetName()) == 0)
				{
					m_pCurSpIdxPtr = (*iter);
					return SMT_ERR_NONE;
				}
				++iter;
			}

			return SMT_ERR_INVALID_PARAM;
		}

		//////////////////////////////////////////////////////////////////////////
		//query
		virtual long             Query(const SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer *pQueryResult) = 0;

		//////////////////////////////////////////////////////////////////////////
		//feature oper
		virtual long             AppendFeature(const SmtFeature *pSmtFeature,bool bClone = false) = 0;

		virtual long             AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bClone = false) = 0;
		virtual long             UpdateFeatureBatch(void) = 0;

		virtual long             UpdateFeature(const SmtFeature *pSmtFeature) = 0;
		virtual long             DeleteFeature(const SmtFeature *pSmtFeature) = 0;

		virtual SmtFeature		*GetFeature()  const = 0;
		virtual SmtFeature		*GetFeature(int index)  const = 0;
		virtual SmtFeature		*GetFeatureByID(uint unID)  const = 0;

		//////////////////////////////////////////////////////////////////////////
		void                     SetLayerFeatureType(SmtFeatureType type) { m_SmtLayerFtType = type;}
		inline SmtFeatureType	 GetLayerFeatureType(void) const { return m_SmtLayerFtType;}

		void					 SetLayerRect(const fRect &lyrRect) 
		{
			m_lyrEnv.MinX = lyrRect.lb.x;
			m_lyrEnv.MinY = lyrRect.lb.y;
			m_lyrEnv.MaxX = lyrRect.rt.x;
			m_lyrEnv.MaxY = lyrRect.rt.y;
		}

		SmtLayerType			 GetLayerType(void) const{return LYR_VECTOR;}

	protected:
		vSpatialIndexPtrs		 m_vSpIdxPtrs;
		SmtSpatialIndex			 *m_pCurSpIdxPtr;

		Envelope				 m_oFilterEnvelope;
		SmtGeometry				 *m_pFilterGeom;
		bool                     m_bIsFilterByGeom;

		SmtFeatureType			 m_SmtLayerFtType;
	};

	class  SmtRasterLayer: public SmtLayer
	{
	public:
		SmtRasterLayer(SmtDataSource *pOwnerDs) :SmtLayer(pOwnerDs)
		{
			;
		}

		virtual ~SmtRasterLayer(void) 
		{
			;
		}

	public:
		//////////////////////////////////////////////////////////////////////////
		//create
		virtual bool             Create(void) = 0;
		virtual bool             Open(const char * szLayerArchiveName) = 0;
		virtual bool             Close(void) = 0;
		virtual bool             Fetch(eSmtFetchType type = FETCH_ALL) = 0;

		//////////////////////////////////////////////////////////////////////////
		//raster oper
		virtual long			 CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fLocRect,long lImageCode) = 0;
		virtual long			 SetRasterRect(const fRect &fLocRect) = 0;

		virtual long			 GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fLocRect,long &lImageCode) const = 0;
		virtual long			 GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fLocRect,long &lImageCode) const = 0;
		virtual long			 GetRasterRect(fRect &fLocRect) const = 0;

		//////////////////////////////////////////////////////////////////////////
		void					 SetLayerRect(const fRect &lyrRect)
		{
			SetRasterRect(lyrRect);

			m_lyrEnv.MinX = lyrRect.lb.x;
			m_lyrEnv.MinY = lyrRect.lb.y;
			m_lyrEnv.MaxX = lyrRect.rt.x;
			m_lyrEnv.MaxY = lyrRect.rt.y;
		}

		SmtLayerType			 GetLayerType(void) const{return LYR_RASTER;}

	};

	class  SmtTileLayer: public SmtLayer
	{
	public:
		SmtTileLayer(SmtDataSource *pOwnerDs):SmtLayer(pOwnerDs)
			,m_lImageCode(-1)
		{
			;
		}

		virtual ~SmtTileLayer(void)
		{

		}

	public:
		//////////////////////////////////////////////////////////////////////////
		//create
		bool					Create(void) = 0;
		bool					Open(const char * szLayerArchiveName) = 0;
		bool					Close(void) = 0;
		bool					Fetch(eSmtFetchType type = FETCH_ALL) = 0;

		void					CalEnvelope(void) = 0;
	
	public:
		//////////////////////////////////////////////////////////////////////////
		//tile set oper
		virtual int              GetTileCount(void) const = 0;

		virtual void             MoveFirst(void) const= 0;
		virtual void             MoveNext(void) const= 0;
		virtual void             MoveLast(void) const= 0;
		virtual void             Delete(void) = 0;
		virtual bool             IsEnd(void) const= 0;

		virtual void			 DeleteAll(void) = 0;

	public:
		virtual long             AppendTile(const SmtTile *pTile,bool bClone = false) = 0;
		virtual long             UpdateTile(const SmtTile *pTile) = 0;
		virtual long             DeleteTile(const SmtTile *pTile) = 0;

		virtual SmtTile			*GetTile()  const = 0;
		virtual SmtTile			*GetTile(int index)  const = 0;
		virtual SmtTile			*GetTileByID(uint unID)  const = 0;

	public:
		SmtLayerType			 GetLayerType(void) const{return LYR_TITLE;}

		//////////////////////////////////////////////////////////////////////////
		void					 SetLayerRect(const fRect &lyrRect)
		{
			m_lyrEnv.MinX = lyrRect.lb.x;
			m_lyrEnv.MinY = lyrRect.lb.y;
			m_lyrEnv.MaxX = lyrRect.rt.x;
			m_lyrEnv.MaxY = lyrRect.rt.y;
		}

	protected:
		long					 m_lImageCode;
	};

	//////////////////////////////////////////////////////////////////////////
	class SmtDataSource
	{
	public:
		SmtDataSource(void) 
		{ 
			m_bOpen = false; 
		}

		virtual ~SmtDataSource(void) 
		{
			m_vLayerInfos.clear();
		}

	public:
		virtual bool             Create(void) = 0;
		virtual bool             Open(void) = 0;
		virtual bool             Close(void) = 0;

		inline	bool			 IsOpen(void) {return m_bOpen;}

	public:	
		virtual SmtDataSource	*Clone(void) const= 0;

		inline void              SetName(const char * szName) { sprintf_s(m_dsInfo.szName,MAX_DS_NAME,szName);}
		inline const char		*GetName(void) const{ return m_dsInfo.szName;}

		inline void              SetUrl(const char * szUrl) { sprintf_s(m_dsInfo.szUrl,MAX_URL_LENGTH,szUrl);}
		inline const char		*GetUrl(void) const{ return m_dsInfo.szUrl;}

		inline void				 SetInfo(const SmtDataSourceInfo &info) { m_dsInfo = info;}
		inline void				 GetInfo(SmtDataSourceInfo &info) const {info = m_dsInfo;}

		inline void              SetType(uint type) { m_dsInfo.unType = type;}
		inline uint		         GetType(void) const{ return m_dsInfo.unType;}

		inline void    			 SetProvider(uint unProvider) {m_dsInfo.unProvider = unProvider;}
		inline uint    			 GetProvider(void) const{return m_dsInfo.unProvider;}

		int                      GetLayerCount(void) const{return m_vLayerInfos.size();}

		void                     GetLayerInfo(SmtLayerInfo &lyrInfo,int index) const
		{
			assert(index < m_vLayerInfos.size() && index > -1); 
			sprintf_s(lyrInfo.szName,MAX_LAYER_NAME,m_vLayerInfos[index].szName);
			sprintf_s(lyrInfo.szArchiveName,MAX_LAYER_ARCHIVE_NAME,m_vLayerInfos[index].szArchiveName);
			lyrInfo.unFeatureType = m_vLayerInfos[index].unFeatureType;
			lyrInfo.unSIType = m_vLayerInfos[index].unSIType;
		}

		void                     GetLayerInfo(SmtLayerInfo &lyrInfo,const char * szName) const
		{
			vector<SmtLayerInfo>::const_iterator iter = m_vLayerInfos.begin() ;

			while (iter != m_vLayerInfos.end())
			{
				if (strcmp((*iter).szName,szName) == 0)
				{
					SmtLayerInfo Info = (*iter) ;
					sprintf_s(lyrInfo.szName,MAX_LAYER_NAME,Info.szName);
					sprintf_s(lyrInfo.szArchiveName,MAX_LAYER_ARCHIVE_NAME,Info.szArchiveName);
					lyrInfo.unFeatureType = Info.unFeatureType;
					lyrInfo.unSIType = Info.unSIType;
					break;
				}
				++iter;
			}
		}

	public:
		virtual SmtVectorLayer	*CreateVectorLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot)  = 0;
		virtual SmtVectorLayer	*OpenVectorLayer(const char *szName) = 0;
		virtual bool             DeleteVectorLayer(const char *szName) = 0;

		virtual SmtRasterLayer	*CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode)  = 0;
		virtual SmtRasterLayer	*OpenRasterLayer(const char *szName) = 0;
		virtual bool             DeleteRasterLayer(const char *szName) = 0;

		virtual SmtTileLayer	*CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode)  = 0;
		virtual SmtTileLayer	*OpenTileLayer(const char *szName) = 0;
		virtual bool             DeleteTileLayer(const char *szName) = 0;

	public:
		static   const char		*GetLayerFeatureTypeName(uint ftType)
		{
			switch(ftType)
			{
			case SmtFeatureType::SmtFtDot:		
				return "DotFcls";
			case SmtFeatureType::SmtFtChildImage:
				return "ChildImageFcls";
			case SmtFeatureType::SmtFtAnno:
				return "AnnoFcls";
			case SmtFeatureType::SmtFtCurve:
				return "CurveFcls";
			case SmtFeatureType::SmtFtSurface:
				return "SurfaceFcls";
			case SmtFeatureType::SmtFtGrid:
				return "GridFcls";
			case SmtFeatureType::SmtFtTin:
				return "TinFcls";
			case SmtFeatureType_Ext::SmtLayer_Ras:
				return "Raster";
			default:
				return "UnKnown";
			}
		}

	protected:
		bool                     m_bOpen;
		SmtDataSourceInfo		 m_dsInfo;

		vector<SmtLayerInfo>	 m_vLayerInfos;
	};
}

#endif //_GIS_SDE_H