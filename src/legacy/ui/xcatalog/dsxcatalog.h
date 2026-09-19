/*
File:    dsxcatalog.h

Desc:    Datasource catalog tree (SmtDSXCatalog) derived from SmtXCatalog.

Version: Version 1.0

Writter: CCL

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XDSCATALOG_H
#define _CATA_XDSCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif

#include "legacy/ui/xcatalog/xcatalog.h"
#include "gis/datasource/mgr/datasource_mgr.h"

using namespace gis;
// SmtXCatalog

namespace ui {
class XCATALOG_EXPORT SmtDSXCatalog : public SmtXCatalog {
  DECLARE_DYNAMIC(SmtDSXCatalog)

 public:
  SmtDSXCatalog();
  virtual ~SmtDSXCatalog();

 public:
  // addtion
  virtual bool InitCreate(void);
  virtual bool EndDestory(void);

  virtual bool CreateContexMenu(void);

  CString GetSelDSName() { return m_strSelDSName; }
  CString GetDSSelLayerName() { return m_strSelDSLayerName; }
  void UpdateCatalogTree(void);

 public:
  virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                      UINT nID);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

  afx_msg void OnDsLayerCreate();
  afx_msg void OnDsLayerDelete();
  afx_msg void OnDsLayerProperty();
  afx_msg void OnDsLayerLoadShp();
  afx_msg void OnDsLayerLoadImage();
  afx_msg void OnDsProperty();

  afx_msg void OnDsDelete();
  afx_msg void OnSvrDsCreate();
  afx_msg void OnSvrDsAppend();
  afx_msg void OnSvrDsSetActive();

  DECLARE_MESSAGE_MAP()

 private:
  void AppendDSNode(SmtDataSource pDS);
  void AppendDSNode(SmtDataSource pDS, const char* preferred_name);

 private:
  CImageList m_imgList;
  HTREEITEM m_hRoot;       // Root tree item
  HTREEITEM m_hDSCatalog;  // Datasource catalog root

  CString m_strSelDSName;       // Selected datasource name
  CString m_strSelDSLayerName;  // Selected layer name
};
}  // namespace ui
#if !defined(XCATALOG_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_CATA_XDSCATALOG_H
