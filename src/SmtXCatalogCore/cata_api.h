/*
File:    cata_api.h

Desc:    SmartGis Catalog API

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_API_H
#define _CATA_API_H

#include "smt_core.h"
#include "gis_sde.h"

using namespace Smt_Core;
using namespace Smt_GIS;
//////////////////////////////////////////////////////////////////////////
//
long		SMT_EXPORT_API		LayerMgrAppend(void);
long		SMT_EXPORT_API		LayerMgrRemove(const char *szSelLayerName);

#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_API_H