// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_CHART_DIAGRAM_DATA_H_
#define LEGACY_UI_SHELL_CHART_DIAGRAM_DATA_H_

#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif

#include "gis/model/layer/layer.h"
#include "gis/model/map/map.h"
#include "legacy/core/macros/macros.h"
#include "legacy/gis/datasource/datasource_mgr.h"

using namespace base;
using namespace gis;

namespace ui {

// In-memory map/datasource backing for leftover chart drawing.
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

#endif  // LEGACY_UI_SHELL_CHART_DIAGRAM_DATA_H_
