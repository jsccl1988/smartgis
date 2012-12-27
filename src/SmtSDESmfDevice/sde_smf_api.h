/*
File:    sde_file_api.h

Desc:    API function

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SHAPEFILE_API_H
#define _SDE_SHAPEFILE_API_H

#include "smt_core.h"
#include "gis_sde.h"

using namespace Smt_Core;
using namespace Smt_GIS;

//////////////////////////////////////////////////////////////////////////
long		WriteSmf(const char *szFile,const vector<SmtLayerInfo> &vLyrInfos);
long		ReadSmf(const char *szFile,vector<SmtLayerInfo> &vLyrInfos);

#if !defined(Export_SmtSDESmfDevice)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSDESmfDeviceD.lib")
#       else
#          pragma comment(lib,"SmtSDESmfDevice.lib")
#	    endif  
#endif

#endif //_SDE_SHAPEFILE_API_H