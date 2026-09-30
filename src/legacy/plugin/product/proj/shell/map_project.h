
#pragma once

#ifndef __AFXWIN_H__
#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy/plugin/product/proj/shell/resource.h"

// CSmtAMMapProjectApp

class CSmtAMMapProjectApp : public CWinApp {
 public:
  CSmtAMMapProjectApp();

 public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
};
