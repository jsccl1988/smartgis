// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_CHART_DIAGRAM_DATA_H_
#define LEGACY_UI_SHELL_CHART_DIAGRAM_DATA_H_

#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif

#include "legacy/gis/feature/model_aliases.h"
#include "legacy/gis/layer/layer.h"
#include "gis/map/map.h"
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
  // Layer*.
  OGRLayer* CreateLayer(const char* szName, fRect& lyrRect,
                        FeatureType ftType = FeatureType::FtDot);
  OGRLayer* GetLayer(const char* szLyrName);
  const OGRLayer* GetLayer(const char* szLyrName) const;
  long DeleteLayer(const char* szLyrName);

 public:
  Map* GetSmtMapPtr(void);
  const Map* GetSmtMapPtr(void) const;

  Map& GetSmtMap(void);
  const Map& GetSmtMap(void) const;

 protected:
  CatalogSource m_memDS;
  Map m_smtMap;
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
