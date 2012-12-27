/*
File:    sde_smf_ogrsupport.h

Desc:    OGR Ö§³Ö

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.10.13

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SMF_OGRSUPPORT_H
#define _SDE_SMF_OGRSUPPORT_H

#include "smt_core.h"
#include "sde_smf.h"
#include "ogrsf_frmts.h"

using namespace Smt_Core;
using namespace Smt_GIS;

#pragma comment(lib,"gdal_i.lib")

void OGRFldTypeToSmtFldType(long ogrType,long &smtType);
void OGRFeaTypeToSmtFeaType(long ogrType,long &smtType);

bool CopyOGRFeaToSmtFea(OGRFeature *pOGRFea,SmtFeature *pSmtFea);

#endif //_SDE_SMF_OGRSUPPORT_H
