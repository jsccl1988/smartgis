/*
File:    sta_diagramdata.h 

Desc:    SmtChart,图锟斤拷

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_GRAPHDATA_H
#define _STA_GRAPHDATA_H
#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "sdb/map/map.h"
#include "sdb/layer/layer.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

using namespace base;
using namespace sdb;

namespace ui
{
	class STAT_CHART_EXPORT SmtDiagramData
	{
	public:
		SmtDiagramData();
		virtual ~SmtDiagramData();

	public:
		virtual long				Init();				
		virtual	long				Clear();

	public:
		SmtLayer*					CreateLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType = SmtFeatureType::SmtFtDot);
		SmtLayer*					GetLayer(const char *szLyrName);
		const SmtLayer*				GetLayer(const char *szLyrName) const;
		long						DeleteLayer(const char *szLyrName);

	public:
		SmtMap *					GetSmtMapPtr(void);
		const SmtMap *				GetSmtMapPtr(void) const;

		SmtMap&						GetSmtMap(void);
		const SmtMap&				GetSmtMap(void) const;

	protected:
		SmtDataSource				m_memDS;
		SmtMap						m_smtMap;		
	};
}

#if !defined(STAT_CHART_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif  
#endif

#endif //_STA_GRAPHDATA_H