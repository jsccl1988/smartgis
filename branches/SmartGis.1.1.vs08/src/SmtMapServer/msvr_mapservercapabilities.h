/*
File:    msvr_mapservercapabilities.h

Desc:    SmartGis ,并发地图服务器驱动能力

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVERCAPABILITIES_H
#define _MSVR_MAPSERVERCAPABILITIES_H

#include "msvr_mapservice.h"
#include "smt_xml.h"
#include "smt_cslock.h"

using namespace Smt_MapService;
using namespace Smt_Core;

namespace Smt_MapServer
{
	class SMT_EXPORT_CLASS SmtCapabilities
	{
	public:
		SmtCapabilities(SmtMapService *pMapService);
		~SmtCapabilities();

	public:
		inline void			SetMapService(SmtMapService *pMapService) {m_pMapService = pMapService;}

	public:
		virtual long		Create(SmtMapService *pMapService = NULL);

		long				Print(FILE* cfile, int depth = 0 );
		long				Save( const char * filename );
		long				GetXML(char *& pXMLBuf,long &lBufLength);
		long				FreeXMLBuf(char *& pXMLBuf);

	protected:
		SmtCSLock			m_cslock;

		TiXmlDocument		m_doc;
		SmtMapService		*m_pMapService;
	};
}

#if !defined(Export_SmtMapServer)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerD.lib")
#       else
#          pragma comment(lib,"SmtMapServer.lib")
#	    endif
#endif

#endif //_MSVR_MAPSERVERCAPABILITIES_H
