// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/toolkit/select_one_dialog.h"

IMPLEMENT_DYNAMIC(CDlgSelectOne, CDialog)

CDlgSelectOne::CDlgSelectOne(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgSelectOne::IDD, pParent) {}

CDlgSelectOne::~CDlgSelectOne() {}

void CDlgSelectOne::DoDataExchange(CDataExchange* pDX) {
  DDX_Control(pDX, IDC_CMB_OBJID, m_cmbIDs);
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgSelectOne, CDialog)
ON_BN_CLICKED(IDOK, &CDlgSelectOne::OnBnClickedOk)
END_MESSAGE_MAP()

void CDlgSelectOne::OnBnClickedOk() {
  UpdateData(TRUE);
  CString strSelID;
  m_cmbIDs.GetLBText(m_cmbIDs.GetCurSel(), strSelID);
  m_unSelID = atoi(strSelID);
  OnOK();
}

BOOL CDlgSelectOne::OnInitDialog() {
  CDialog::OnInitDialog();
  update_id_cmb();
  return TRUE;
}

void CDlgSelectOne::update_id_cmb() {
  CString strID;
  m_cmbIDs.ResetContent();
  for (size_t i = 0; i < m_vIDs.size(); i++) {
    strID.Format("%d", m_vIDs[i]);
    m_cmbIDs.AddString(strID);
  }

  if (!m_vIDs.empty()) {
    m_cmbIDs.SetCurSel(0);
  }
}
