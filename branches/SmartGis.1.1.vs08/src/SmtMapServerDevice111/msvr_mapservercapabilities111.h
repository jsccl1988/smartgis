/*
File:    msvr_mapservercapabilities111.h

Desc:    SmartGis ,并发地图服务器驱动能力

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVERCAPABILITIES111_H
#define _MSVR_MAPSERVERCAPABILITIES111_H

#include "msvr_mapservice.h"
#include "smt_xml.h"
#include "smt_cslock.h"
#include "msvr_mapservercapabilities.h"

using namespace Smt_MapService;
using namespace Smt_Core;

namespace Smt_MapServer
{
	class SMT_EXPORT_CLASS SmtWMSCapabilities111:public SmtCapabilities
	{
	public:
		SmtWMSCapabilities111(SmtMapService *pMapService);
		~SmtWMSCapabilities111();

	public:
		virtual long		Create(SmtMapService *pMapService = NULL);

	protected:
		////////////////////////////////////////////////////////////////////////////////////
		//root
		long				Build();

		////////////////////////////////////////////////////////////////////////////////////
		//+
		long				BuildService(TiXmlElement * Parent);
		long				BuildCapability(TiXmlElement *Parent);

		////////////////////////////////////////////////////////////////////////////////////
		//++
		//service

		//capability
		long				BuildRequest(TiXmlElement * Parent);
		long				BuildLayer(TiXmlElement * Parent);

		////////////////////////////////////////////////////////////////////////////////////
		//+++
		//request
		long				BuildGetCapabilities(TiXmlElement * Parent);
		long				BuildGetMap(TiXmlElement * Parent);

		//+++
		//layer
		long				BuildSRS(TiXmlElement * Parent);
		long				BuildLayers(TiXmlElement * Parent);
		long				BuildLayer(TiXmlElement * Parent,const SmtLayer * pLayer);

	};
}

#if !defined(Export_SmtMapServerDevice111)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerDevice111D.lib")
#       else
#          pragma comment(lib,"SmtMapServerDevice111.lib")
#	    endif
#endif

#endif //_MSVR_MAPSERVERCAPABILITIES111_H
