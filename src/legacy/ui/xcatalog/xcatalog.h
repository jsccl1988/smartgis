/*
File:    cata_xcatalog.h

Desc:    SmtXCatalog,Smt Catalog 锟教筹拷锟斤拷CTreeCtrl

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _CATA_XCATALOG_H
#define _CATA_XCATALOG_H
#if defined(XCATALOG_EXPORTS)
#define XCATALOG_EXPORT __declspec(dllexport)
#else
#define XCATALOG_EXPORT __declspec(dllimport)
#endif

#include "base/core/core.h"
// SmtXCatalog
namespace ui {
class XCATALOG_EXPORT SmtXCatalog : public CTreeCtrl {
  DECLARE_DYNAMIC(SmtXCatalog)

 public:
  SmtXCatalog();
  virtual ~SmtXCatalog();

 public:
  // addtion
  virtual bool InitCreate(void);
  virtual bool EndDestory(void);

  virtual bool CreateContexMenu(void) { return true; }

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

 protected:
  HMENU m_hContexMenu;
};
}  // namespace ui
#if !defined(XCATALOG_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_CATA_XCATALOG_H
