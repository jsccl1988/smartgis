/*
File:    sde_ws.h

Desc:    SmtWSDataSource, SmtWSTileLayer,Web 数据源，支持WMS WFS

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_WS_H
#define _SDE_WS_H

#include "gis_sde.h"

using namespace Smt_GIS;

#define  MAX_LAYER_URL     (MAX_FILE_PATH)

const	string		C_STR_SDE_WSDEVICE_LOG = "SmtSDEWSDevice";

namespace Smt_SDEWS
{
	class  SmtWSTileLayer: public SmtTileLayer
	{
	public:
		SmtWSTileLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtWSTileLayer(void);

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
		int						GetTileCount(void) const{return m_pMemLayer->GetTileCount();}

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

	public:
		void					SetLayerRect(const fRect &lyrRect);

	protected:
		char                    m_szLayerURL[MAX_LAYER_URL];
		SmtTileLayer			*m_pMemLayer;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtWSDataSource:public SmtDataSource
	{
	public:
		SmtWSDataSource(void);
		virtual ~SmtWSDataSource(void);

	public:
		bool					Create(void);
		bool                    Open();
		bool                    Close();

		SmtDataSource			*Clone(void) const;

	public:
		//unsupport
		SmtVectorLayer			*CreateVectorLayer(const char *pszName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot) {return NULL;}
		SmtVectorLayer			*OpenVectorLayer(const char *pszLayerFileName){return NULL;}
		bool                    DeleteVectorLayer(const char *pszName){return false;}

		SmtRasterLayer			*CreateRasterLayer(const char *szName,fRect &lyrRect,long lDecodeType){return NULL;}
		SmtRasterLayer			*OpenRasterLayer(const char *szName){return NULL;}
		bool					DeleteRasterLayer(const char *szName){return false;}

		//
		SmtTileLayer			*CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode);
		SmtTileLayer			*OpenTileLayer(const char *szName);
		bool					 DeleteTileLayer(const char *szName);
	};
}

#if !defined(Export_SmtSDEWSDevice)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSDEWSDeviceD.lib")
#       else
#          pragma comment(lib,"SmtSDEWSDevice.lib")
#	    endif  
#endif

#endif //_SDE_WS_H