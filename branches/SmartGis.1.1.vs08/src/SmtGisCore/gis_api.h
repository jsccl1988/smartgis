/*
File:    gis_api.h

Desc:    SmartGis Gis API

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_API_H
#define _GIS_API_H

#include "smt_core.h"
#include "gis_sde.h"

using namespace Smt_Core;
using namespace Smt_GIS;
//////////////////////////////////////////////////////////////////////////
//
long		SMT_EXPORT_API		CopyLayer(SmtLayer* pTarLayer,SmtLayer* pSrcLayer,bool bClone = true,bool bCheckFeaType = false);
long		SMT_EXPORT_API		CopyLayer(SmtVectorLayer* pTarLayer,SmtVectorLayer* pSrcLayer,bool bClone = true,bool bCheckFeaType = false);
long		SMT_EXPORT_API		CopyLayer(SmtRasterLayer* pTarLayer,SmtRasterLayer* pSrcLayer,bool bClone = true,bool bCheckFeaType = false);

long		SMT_EXPORT_API		Points2MultiPoint(SmtVectorLayer* pLayer,string strStyle);

long		SMT_EXPORT_API		GetQueryRs(int geomType,int feaType);

#if !defined(Export_SmtGisCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisCoreD.lib")
#       else
#          pragma comment(lib,"SmtGisCore.lib")
#	    endif  
#endif

#endif //_GIS_API_H