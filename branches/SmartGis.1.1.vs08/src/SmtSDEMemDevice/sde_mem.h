/*
File:    sde_mem.h

Desc:    SmtMemDataSource,SmtMemVecLayer SmtMemRasLayer,内存数据源

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_MEM_H
#define _SDE_MEM_H

#include "gis_sde.h"

using namespace Smt_GIS;

namespace Smt_SDEMem
{
	class SMT_EXPORT_CLASS SmtMemVecLayer:public SmtVectorLayer
	{
	public:
		SmtMemVecLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtMemVecLayer(void);

	public:
		bool                    Create(void);
		bool                    Open(const char *szName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		//////////////////////////////////////////////////////////////////////////
		int                     GetFeatureCount(void) const{ return m_vSmtFeatures.size();}

		//iter record
		void                    MoveFirst(void) const;
		void                    MoveNext(void) const;
		void                    MoveLast(void) const;
		void                    Delete(void);
		bool                    IsEnd(void) const;

		void					DeleteAll(void);

		//////////////////////////////////////////////////////////////////////////
		//spatial index
		long					CreateSpatialIndex(const char *szName,uint type){return SMT_ERR_NONE;}

		//query
		long                    Query(const SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer * pQueryResult);

		//feature
		long                    AppendFeature(const SmtFeature *pSmtFeature,bool bclone = false);

		long					AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bClone = false);
		long					UpdateFeatureBatch(void) {return SMT_ERR_NONE;}

		long                    UpdateFeature(const SmtFeature *pSmtFeature);
		long                    DeleteFeature(const SmtFeature *pSmtFeature);

		SmtFeature				*GetFeature() const ;
		SmtFeature				*GetFeature(int index) const ;
		SmtFeature				*GetFeatureByID(uint unID) const ;

		//////////////////////////////////////////////////////////////////////////
		void                    CalEnvelope(void);

	protected:
		vector<SmtFeature*>		m_vSmtFeatures;
		mutable int             m_nIteratorIndex;
	};

	class  SmtMemRasLayer: public SmtRasterLayer
	{
	public:
		SmtMemRasLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtMemRasLayer(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//create
		bool					Create(void);
		bool					Open(const char * szLayerArchiveName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		void					CalEnvelope(void);
		//////////////////////////////////////////////////////////////////////////
		//raster oper
		long					CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fRasterRect,long lImageCode);
		long					SetRasterRect(const fRect &fLocRect);

		long					GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const;
		long					GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fLocRect,long &lImageCode) const;
		
		long					GetRasterRect(fRect &fLocRect) const;

	protected:
		fRect					m_fRasterRect;

		char					*m_pRasterBuf;
		long					m_lRasterBufSize;
		long					m_lCodeType;
	};

	class  SmtMemTileLayer: public SmtTileLayer
	{
	public:
		SmtMemTileLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtMemTileLayer(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//create
		bool					Create(void);
		bool					Open(const char * szLayerArchiveName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		void					CalEnvelope(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//title set oper
		int						GetTileCount(void) const{return m_vTilePtrs.size();}

		void					MoveFirst(void) const;
		void					MoveNext(void) const;
		void					MoveLast(void) const;
		void					Delete(void);
		bool					IsEnd(void) const;

		void					DeleteAll(void);

	public:
		long					AppendTile(const SmtTile *pTile,bool bClone = false);
		long					UpdateTile(const SmtTile *pTile);
		long					DeleteTile(const SmtTile *pTile);

		SmtTile					*GetTile()  const;
		SmtTile					*GetTile(int index)  const ;
		SmtTile					*GetTileByID(uint unID)  const ;

	protected:
		vector<SmtTile*>		m_vTilePtrs;
		mutable int             m_nIteratorIndex;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtMemDataSource:public SmtDataSource
	{
	public:
		SmtMemDataSource(void);
		virtual ~SmtMemDataSource(void);

	public:
		bool					Create(void);
		bool                    Open();
		bool                    Close();

		SmtDataSource			*Clone(void) const;

	public:
		SmtVectorLayer			*CreateVectorLayer(const char *pszName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot);
		SmtVectorLayer			*OpenVectorLayer(const char *pszLayerFileName);
		bool                    DeleteVectorLayer(const char *pszName);

		SmtRasterLayer			*CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode);
		SmtRasterLayer			*OpenRasterLayer(const char *szName);
		bool					DeleteRasterLayer(const char *szName);

		SmtTileLayer			*CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode);
		SmtTileLayer			*OpenTileLayer(const char *szName);
		bool					DeleteTileLayer(const char *szName);
	};
}

#if !defined(Export_SmtSDEMemDevice)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSDEMemDeviceD.lib")
#       else
#          pragma comment(lib,"SmtSDEMemDevice.lib")
#	    endif  
#endif

#endif //_SDE_MEM_H