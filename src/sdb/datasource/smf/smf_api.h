/*
File:    sde_file_api.h

Desc:    API function

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SHAPEFILE_API_H
#define _SDE_SHAPEFILE_API_H

#include "base/core/core.h"
#include "sdb/layer/layer.h"

using namespace base;
using namespace sdb;

//////////////////////////////////////////////////////////////////////////
long write_smf(const char* szFile, const vector<SmtLayerInfo>& vLyrInfos);
long read_smf(const char* szFile, vector<SmtLayerInfo>& vLyrInfos);

#endif  // _SDE_SHAPEFILE_API_H