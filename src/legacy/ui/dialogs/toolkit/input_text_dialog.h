// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/ui/dialogs/resource.h"

// Toolkit modal: single-line text prompt (≈ ui/views/dialogs/InputTextDialog).
class CDlgInputText : public CDialog {
  DECLARE_DYNAMIC(CDlgInputText)

 public:
  CDlgInputText(CWnd* pParent = NULL);
  ~CDlgInputText() override;

  enum { IDD = IDD_DLG_INPUTTEXT };

 protected:
  void DoDataExchange(CDataExchange* pDX) override;

  DECLARE_MESSAGE_MAP()

  afx_msg void OnBnClickedOk();

 public:
  CString m_strText;
};
