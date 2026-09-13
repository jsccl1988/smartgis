// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#ifndef __AFXWIN_H__
#error "include stdafx.h before this file to generate the PCH"
#endif

#include "plugin/legacy/orthogrid/resource.h"

class CSmtAMOrthogridApp : public CWinApp {
 public:
  CSmtAMOrthogridApp();

 public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
};
