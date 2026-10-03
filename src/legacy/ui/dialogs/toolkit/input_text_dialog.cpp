// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/toolkit/input_text_dialog.h"

IMPLEMENT_DYNAMIC(CDlgInputText, CDialog)

CDlgInputText::CDlgInputText(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgInputText::IDD, pParent) {}

CDlgInputText::~CDlgInputText() {}

void CDlgInputText::DoDataExchange(CDataExchange* pDX) {
  DDX_Text(pDX, IDC_EDIT_TEXT, m_strText);
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgInputText, CDialog)
ON_BN_CLICKED(IDOK, &CDlgInputText::OnBnClickedOk)
END_MESSAGE_MAP()

void CDlgInputText::OnBnClickedOk() {
  UpdateData(TRUE);
  OnOK();
}
