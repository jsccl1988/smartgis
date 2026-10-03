// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_VIEWPORT_VIEW_2D_EDIT_H_
#define LEGACY_UI_MAP_VIEWPORT_VIEW_2D_EDIT_H_

#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "legacy/ui/map/viewport/view_2d.h"

namespace gis {
class MapEditSession;
}

namespace ui {

// 2D edit CView: browse host + AppendFeature tool + MapEditSession ViewHost.
class XVIEW_EXPORT Smt2DEditXView : public Smt2DXView {
  DECLARE_DYNCREATE(Smt2DEditXView)

 protected:
  Smt2DEditXView();
  virtual ~Smt2DEditXView();

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  void SetOperMap(SmtMap* pSmtMap);

 protected:
  bool InitCreate(void);
  bool EndDestory(void);
  bool CreateContexMenu();
  bool CreateMainMenu();

  bool CreateTools(void);

 protected:
  SmtBaseTool* m_pAppendFeaTool;
  gis::MapEditSession* m_pMapEdits;
};

}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // LEGACY_UI_MAP_VIEWPORT_VIEW_2D_EDIT_H_
