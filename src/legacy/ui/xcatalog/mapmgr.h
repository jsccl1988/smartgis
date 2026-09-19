/*
File:    cata_mapmgr.h

Desc:    SmtMapMgr,锟斤拷图锟侥碉拷锟斤拷锟斤拷锟斤拷

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_MAPMGR_H
#define _CATA_MAPMGR_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif

#include <mutex>

#include "base/core/core.h"
#include "base/core/env_struct.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"

using namespace gis;

namespace ui {
class XCATALOG_EXPORT SmtMapMgr {
 private:
  SmtMapMgr(void);

 public:
  virtual ~SmtMapMgr(void);

 public:
  static SmtMapMgr *get_singleton_ptr(void);
  static void DestoryInstance(void);

 public:
  static bool NewMap(SmtMap *&pMap, const char *szMapName);
  static bool OpenMap(SmtMap *pMap, const char *szMapFile);
  static bool SaveMapAs(SmtMap *pMap, const char *szFilePath);

 public:
  SmtMap *GetSmtMapPtr(void);
  const SmtMap *GetSmtMapPtr(void) const;

  SmtMap &GetSmtMap(void);
  const SmtMap &GetSmtMap(void) const;

  bool NewMap(const char *szMapName);
  bool OpenMap(const char *szMapFile);
  bool CloseMap();
  bool SaveMap();
  bool SaveMapAs(const char *szFilePath);

  bool AppendLayer(SmtLayer *pLayer);
  bool AppendLayer(OGRLayer *pLayer);
  bool DeleteLayer(const char *szName);
  SmtLayer *GetLayer(int index);
  SmtLayer *GetLayer(const char *szName);

  bool SetActiveLayer(const char *szName);
  SmtLayer *GetActiveLayer(void);

  bool AppendFeature(SmtFeature *pFeature, bool bIsClone = false);

 public:
  // xview registers this so catalog does not link SmtXViewCore (GN cycle).
  typedef void (*Smt2DXViewNotifyFn)(void *p2DXView, SmtMap *pMap);
  void Set2DXViewNotify(Smt2DXViewNotifyFn fn);

  bool Register2DXView(void *p2DXView);
  bool Unregister2DXView(void *p2DXView);

  bool RegisterMapCatalog(void *pMapCatalog);
  bool UnregisterMapCatalog(void *pMapCatalog);

  // Public so app bootstrap (InitSmtMap) can refresh EDIT1/DS1 after
  // AppendLayer.
  bool Update2DXView(void);
  bool UpdateMapCatalog(void);

 protected:
 private:
  SmtMap *m_pSmtMap;
  string m_strMDocPath;
  Smt2DXViewNotifyFn m_fn2DXViewNotify;
  vector<void *> m_v2DXViewPtrs;
  vector<void *> m_vMapCatalogPtrs;
#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;  // 锟斤拷锟竭程帮拷全
#endif

 private:
  static SmtMapMgr *m_pSingleton;
};
}  // namespace ui

#if !defined(XCATALOG_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_CATA_MAPMGR_H