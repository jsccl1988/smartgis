/*
File:    sde_smf.h

Desc:    SmtSmfDataSource,SmtSmfVecLayer SmtSmfRasLayer,����Դ+ͼ��(shp�ļ�)

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SMF_H
#define _SDE_SMF_H

#include "sdb/datasource/smf/sde_smf_export.h"
#include "sdb/layer/layer.h"
using namespace sdb;

#define  MAX_LAYER_FILE_NAME     (MAX_FILE_PATH)

const	string		C_STR_SDE_SMFDEVICE_LOG = "SmtSDESmfDevice";

namespace sdb
{
	class SDE_SMF_EXPORT SmtSmfVecLayer:public SmtVectorLayer
	{
	public:
		SmtSmfVecLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtSmfVecLayer(void);

	public:
		bool                    Create(void);
		bool                    Open(const char *szLayerFileName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		//////////////////////////////////////////////////////////////////////////
		int                     GetFeatureCount(void) const {return m_pMemLayer->GetFeatureCount();}

		void                    MoveFirst(void) const;
		void                    MoveNext(void) const;
		void                    MoveLast(void) const;
		void                    Delete(void);
		bool                    IsEnd(void) const;

		void					DeleteAll(void);

		//query
		long                    Query(const SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer * pQueryResult);

		//////////////////////////////////////////////////////////////////////////
		//spatial index
		long					CreateSpatialIndex(const char *szName,uint type){return SMT_ERR_NONE;}

		//feature
		long                    AppendFeature(const SmtFeature *pSmtFeature,bool bClone = false) ;

		long					AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bClone = false) {return SMT_ERR_NONE;};
		long					UpdateFeatureBatch(void) {return SMT_ERR_NONE;};

		long                    UpdateFeature(const SmtFeature *pSmtFeature);
		long                    DeleteFeature(const SmtFeature *pSmtFeature);

		SmtFeature				*GetFeature() const ;
		SmtFeature				*GetFeature(int index) const ;
		SmtFeature				*GetFeatureByID(uint unID) const ;

		//////////////////////////////////////////////////////////////////////////
		void                    CalEnvelope(void);
		void					SetLayerRect(const fRect &lyrRect);

	private:
		bool					PreReadShp(const char * szShpName);
		bool                    ReadShp(const char * szShpName);

	protected:
		char                    m_szLayerFileName[MAX_LAYER_FILE_NAME];
		SmtVectorLayer			*m_pMemLayer;
	};

	class SDE_SMF_EXPORT SmtSmfRasLayer : public SmtRasterLayer
	{
	public:
		SmtSmfRasLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtSmfRasLayer(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//create
		bool					Create(void);
		bool					Open(const char * szLayerArchiveName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		void					CalEnvelope(void);
		void					SetLayerRect(const fRect &lyrRect);
		//////////////////////////////////////////////////////////////////////////
		//raster oper
		long					CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fRasterRect,long lImageCode);
		long					SetRasterRect(const fRect &fLocRect);

		long					GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const;
		long					GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fLocRect,long &lImageCode) const;

		long					GetRasterRect(fRect &fLocRect) const;

	protected:
		char                    m_szLayerFileName[MAX_LAYER_FILE_NAME];
		SmtRasterLayer			*m_pMemLayer;
	};

	//////////////////////////////////////////////////////////////////////////
	class SDE_SMF_EXPORT SmtSmfDataSource:public SmtDataSource
	{
	public:
		SmtSmfDataSource(void);
		virtual ~SmtSmfDataSource(void);

	public:
		bool					Create(void);
		bool                    Open(void);
		bool                    Close(void);

		SmtDataSource			*clone(void) const;

	public:
		SmtVectorLayer			*CreateVectorLayer(const char *pszName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot);
		SmtVectorLayer			*OpenVectorLayer(const char *szLayerName);
		bool                    DeleteVectorLayer(const char *pszName);

		SmtRasterLayer			*CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode);
		SmtRasterLayer			*OpenRasterLayer(const char *szLayerName);
		bool					DeleteRasterLayer(const char *szName);

		//unsupport
		SmtTileLayer			*CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode) {return NULL;}
		SmtTileLayer			*OpenTileLayer(const char *szName) {return NULL;}
		bool					DeleteTileLayer(const char *szName) {return false;}
	};
}

#endif  // _SDE_SMF_H