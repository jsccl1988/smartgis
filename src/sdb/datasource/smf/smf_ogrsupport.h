// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _SDE_SMF_OGRSUPPORT_H
#define _SDE_SMF_OGRSUPPORT_H

#include "base/core/core.h"
#include "sdb/datasource/smf/smf.h"
#include "ogrsf_frmts.h"
#include "gdal.h"

using namespace base;
using namespace sdb;

void ogr_fld_type_to_smt_fld_type(long ogrType, long& smtType);
void ogr_fea_type_to_smt_fea_type(long ogrType, long& smtType);

bool copy_ogr_fea_to_smt_fea(OGRFeature* pOGRFea, SmtFeature* pSmtFea);

#endif  // _SDE_SMF_OGRSUPPORT_H
