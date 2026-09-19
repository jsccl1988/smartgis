/*
File:    cata_scenemgr.h

Desc:    SmtMapMgr,锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_SCENEMGR_H
#define _CATA_SCENEMGR_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif

#include <mutex>

#include "base/core/core.h"
#include "base/core/env_struct.h"
#include "legacy/render/scene3d/bl3d_scene.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"

using namespace gis;
using namespace render;

namespace ui {
class XCATALOG_EXPORT SmtSceneMgr {
 private:
  SmtSceneMgr(void);

 public:
  virtual ~SmtSceneMgr(void);

 public:
  static SmtSceneMgr* get_singleton_ptr(void);
  static void DestoryInstance(void);

 public:
  SmtScene* GetScenePtr(void);
  const SmtScene* GetScenePtr(void) const;

  SmtScene& GetScene(void);
  const SmtScene& GetScene(void) const;

  bool AttachScene(SmtScene* pScene);
  SmtScene* DettachScene(void);

 public:
  void Add3DObject(Smt3DObject* p3DObject);
  Smt3DObject* Get3DObject(int index);
  void Remove3DObject(Smt3DObject* p3DObject);
  void Remove3DObject(int index);
  void Get3DObjectPtrs(vSmt3DObjectPtrs& v3DObjectPtrs);

  void CreateOctTreeSceneMgr(void);

 public:
  bool Register3DXView(void* p3DXView);
  bool Unregister3DXView(void* p3DXView);

  bool Register3DObjCatalog(void* p3DObjCatalog);
  bool Unregister3DObjCatalog(void* p3DObjCatalog);

 protected:
  bool Update3DXView(void);
  bool Update3DObjCatalog(void);

 private:
  SmtScene* m_pScene;

  vector<void*> m_v3DXViewPtrs;
  vector<void*> m_v3DObjCatalogPtrs;
#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;  // 锟斤拷锟竭程帮拷全
#endif

 private:
  static SmtSceneMgr* m_pSingleton;
};
}  // namespace ui

#if !defined(XCATALOG_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_CATA_SCENEMGR_H