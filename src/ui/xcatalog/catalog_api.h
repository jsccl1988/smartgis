/*
File:    cata_api.h

Desc:    SmartGis Catalog API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_API_H
#define _CATA_API_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "sdb/layer/layer.h"

using namespace base;
using namespace sdb;
//////////////////////////////////////////////////////////////////////////
//
long		XCATALOG_EXPORT		LayerMgrAppend(void);
long		XCATALOG_EXPORT		LayerMgrRemove(const char *szSelLayerName);

#if !defined(XCATALOG_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"xcatalogD.lib")
#       else
#          pragma comment(lib,"xcatalog.lib")
#	    endif
#endif

#endif //_CATA_API_H