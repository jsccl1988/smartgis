#ifndef _MSVR_MGR_API_H
#define _MSVR_MGR_API_H

#include "smt_core.h"

#include "msvr_mapservice.h"

using namespace Smt_MapService;

long				MSVR_MGR_ViewMap(string	 strMDocFile);
long				MSVR_MGR_ViewTileMap(SmtMapService	*pMapService);

#endif //_MSVR_MGR_API_H