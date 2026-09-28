
#pragma once

#ifndef __AFXWIN_H__
#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy/plugin/model3d/resource.h"

// CSmtAM3DModelCreaterApp

class CSmtAM3DModelCreaterApp : public CWinApp {
 public:
  CSmtAM3DModelCreaterApp();

 public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
};
