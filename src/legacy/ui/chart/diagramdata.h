/*
File:    sta_diagramdata.h

Desc:    SmtChart,图锟斤拷

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_GRAPHDATA_H
#define _STA_GRAPHDATA_H
#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif

#include "base/core/core.h"
#include "gis/datasource/mgr/datasource_mgr.h"
#include "gis/layer/layer.h"
#include "gis/map/map.h"

using namespace base;
using namespace gis;

namespace ui {
class STAT_CHART_EXPORT SmtDiagramData {
 public:
  SmtDiagramData();
  virtual ~SmtDiagramData();

 public:
  virtual long Init();
  virtual long Clear();

 public:
  // Vector chart layers are OGR (MapLayer::from_ogr); do not return leftover
  // SmtLayer*.
  OGRLayer* CreateLayer(const char* szName, fRect& lyrRect,
                        SmtFeatureType ftType = SmtFeatureType::SmtFtDot);
  OGRLayer* GetLayer(const char* szLyrName);
  const OGRLayer* GetLayer(const char* szLyrName) const;
  long DeleteLayer(const char* szLyrName);

 public:
  SmtMap* GetSmtMapPtr(void);
  const SmtMap* GetSmtMapPtr(void) const;

  SmtMap& GetSmtMap(void);
  const SmtMap& GetSmtMap(void) const;

 protected:
  SmtDataSource m_memDS;
  SmtMap m_smtMap;
};
}  // namespace ui

#if !defined(STAT_CHART_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_STA_GRAPHDATA_H