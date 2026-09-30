
#pragma once

#ifndef __AFXWIN_H__
#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy/plugin/product/print/shell/resource.h"

// CSmtAMMapPrintApp

class CSmtAMMapPrintApp : public CWinApp {
 public:
  CSmtAMMapPrintApp();

 public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
};
