/*
File:    sde_ado.h

Desc:    SmtAdoDataSource,SmtAdoVecLayer SmtAdoRasLayer,数据源+图层(数据库)

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_ADO_H
#define _SDE_ADO_H

#include "gis_sde.h"
using namespace Smt_GIS;

#include "ado_adorecordset.h"
#include "ado_adoconnection.h"

using namespace Smt_Ado;

const	string		C_STR_SDE_ADODEVICE_LOG = "SmtSDEAdoDevice";

#define  MAX_LAYER_TB_NAME                  MAX_LAYER_ARCHIVE_NAME
#define  DS_TABLE_NAME                      ("SMT_DS_TB")
#define  DS_FLD_LYR_NAME                    ("layer_name")
#define  DS_FLD_LYR_TABLENAME               ("layer_tablename")
#define  DS_FLD_LYR_TYPE					("layer_type")
#define  DS_FLD_LYR_SPIDXTYPE				("layer_spidx_type")

#define  DS_FLD_LYR_XMIN					("layer_xmin")
#define  DS_FLD_LYR_YMIN					("layer_xmin")
#define  DS_FLD_LYR_XMAX					("layer_xmin")
#define  DS_FLD_LYR_YMAX					("layer_xmin")

#define  SQL_STRING_BUF_LENGTH              TEMP_BUFFER_SIZE

namespace Smt_SDEAdo
{
	typedef						SmtAttribute			SmtFieldCollect;
	
	class SMT_EXPORT_CLASS SmtAdoVecLayer:public SmtVectorLayer
	{
	public:
		SmtAdoVecLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtAdoVecLayer(void);

	public:
		bool					Open(const char *szLayerTableName);
		bool					Close(void);
		bool					Fetch(eSmtFetchType type = FETCH_ALL);

		//////////////////////////////////////////////////////////////////////////
		int                     GetFeatureCount(void)  const;

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
		virtual long            AppendFeature(const SmtFeature *pSmtFeature,bool bclone = false) = 0;

		long					UpdateFeatureBatch(void);

		virtual long            UpdateFeature(const SmtFeature *pSmtFeature) = 0;
		virtual long            DeleteFeature(const SmtFeature *pSmtFeature);

		SmtFeature				*GetFeature() const ;
		SmtFeature				*GetFeature(int index) const ;
		SmtFeature				*GetFeatureByID(uint unID) const ;

		//////////////////////////////////////////////////////////////////////////
		void                    CalEnvelope(void);
		void					SetLayerRect(const fRect &lyrRect);

	public:
		virtual	void			SetDefaultAttFields(void) = 0;
		virtual	void			SetDefaultGeomFields(void) = 0;
		virtual	void			SetTableFields(void) = 0;

		virtual void            GetFeature(SmtFeature *pSmtFeature) = 0;

	protected:
	    SmtVectorLayer			*m_pMemLayer;
		SmtFieldCollect			*m_pGeomFieldCollect;
		SmtFieldCollect			*m_pTableFieldCollect;

		SmtAdoRecordSet			m_SmtRecordset;
		char                    m_szLayerTableName[MAX_LAYER_TB_NAME];
	};

	class  SmtAdoRasLayer: public SmtRasterLayer
	{
	public:
		SmtAdoRasLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtAdoRasLayer(void);

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

		//////////////////////////////////////////////////////////////////////////
	public:
		virtual	void			SetDefaultAttFields(void);
		virtual	void			SetDefaultGeomFields(void);
		virtual	void			SetTableFields(void);

	private:
		long                    SyncWrite(void);
		long                    SyncRead(void);

	protected:
		SmtRasterLayer			*m_pMemLayer;
		SmtFieldCollect			*m_pGeomFieldCollect;
		SmtFieldCollect			*m_pTableFieldCollect;

		SmtAdoRecordSet			m_SmtRecordset;
		char                    m_szLayerTableName[MAX_LAYER_TB_NAME];
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtAdoDataSource:public SmtDataSource
	{
	public:
		SmtAdoDataSource(void);
		virtual ~SmtAdoDataSource(void);

	public:
		bool					Create(void);
		bool                    Open();
		bool                    Close();

		SmtDataSource			*Clone(void) const;

	public:
		SmtAdoConnection		&GetConnection(void) {return m_SmtConnection;}

	public:
		SmtVectorLayer			*CreateVectorLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot);
		SmtVectorLayer			*OpenVectorLayer(const char *szLayerTableName);
		bool                    DeleteVectorLayer(const char *szName);

		SmtRasterLayer			*CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode);
		SmtRasterLayer			*OpenRasterLayer(const char *szName);
		bool					DeleteRasterLayer(const char *szName);

		//unsupport
		SmtTileLayer			*CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode) {return NULL;}
		SmtTileLayer			*OpenTileLayer(const char *szName) {return NULL;}
		bool					 DeleteTileLayer(const char *szName) {return false;}

	public:
		bool                    CreateLayerTable(SmtLayerInfo &info,SmtAttribute   *pAtt);

		bool                    IsTableExist(const char *szTableName);
		bool                    CreateTable(const char *szTableName,SmtAttribute   *pAtt);
		inline  bool            OpenTable(const char *szTableName,SmtAdoRecordSet *pSmtRecordset,CursorTypeEnum CursorType = adOpenStatic, LockTypeEnum LockType = adLockOptimistic);
		inline  bool            CloseTable(const char *szTableName,SmtAdoRecordSet *pSmtRecordset);
		inline  bool            DropTable(const char *szTableName);
		inline  bool            DropTrigger(const char *szTableName);
		inline  bool            ClearTable(const char *szTableName);

		inline  void            CetTableFldCreatingString(string &str,SmtField &smtFld);

	protected:
		bool                    CreateDsTable(void);

		bool                    GetLayerTableInfos(void);

		void                    GetLayerTableInfo(SmtLayerInfo &info);
		bool                    GetLayerTableInfoByLayerName(SmtLayerInfo &info,const char * szName);

		bool                    AppendLayerTableInfo(SmtLayerInfo &info);
		bool                    DeleteLayerTableInfo(SmtLayerInfo &info);
		bool                    UpdateLayerTableInfo(SmtLayerInfo &info);

	protected:
		SmtAdoConnection		m_SmtConnection;
		SmtAdoRecordSet			m_SmtRecordset;
	};
}

#if !defined(Export_SmtSDEAdoDevice)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSDEAdoDeviceD.lib")
#       else
#          pragma comment(lib,"SmtSDEAdoDevice.lib")
#	    endif  
#endif

#endif //_SDE_ADO_H