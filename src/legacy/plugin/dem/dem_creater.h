
#pragma once

#ifndef __AFXWIN_H__
#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy/plugin/dem/resource.h"

// CSmtAMDemCreaterApp

class CSmtAMDemCreaterApp : public CWinApp {
 public:
  CSmtAMDemCreaterApp();

 public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
};
