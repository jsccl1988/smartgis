/*
File:    sta_diagramdata.h 

Desc:    SmtChart,Í¼±í

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_GRAPHDATA_H
#define _STA_GRAPHDATA_H

#include "smt_core.h"
#include "gis_map.h"
#include "gis_sde.h"

using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_StaDiagram
{
	class SMT_EXPORT_CLASS SmtDiagramData
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
		SmtDataSource				*m_pMemDS;
		SmtMap						m_smtMap;		
	};
}

#if !defined(Export_SmtStaDiagram)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaDiagramD.lib")
#       else
#          pragma comment(lib,"SmtStaDiagram.lib")
#	    endif  
#endif

#endif //_STA_GRAPHDATA_H