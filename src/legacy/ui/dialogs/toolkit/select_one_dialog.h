// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/core/types/types.h"
#include "legacy/ui/dialogs/resource.h"

// Toolkit modal: pick one id from a list (≈ ui/views/dialogs/SelectOneDialog).
class CDlgSelectOne : public CDialog {
  DECLARE_DYNAMIC(CDlgSelectOne)

 public:
  CDlgSelectOne(CWnd* pParent = NULL);
  ~CDlgSelectOne() override;

  enum { IDD = IDD_DLG_SELECT_ONE };

 protected:
  void DoDataExchange(CDataExchange* pDX) override;
  BOOL OnInitDialog() override;

  DECLARE_MESSAGE_MAP()

 public:
  afx_msg void OnBnClickedOk();

 public:
  void set_id_list(vector<uint>& ids) { m_vIDs = ids; }
  uint selected_id() const { return m_unSelID; }

  void update_id_cmb();

 protected:
  vector<uint> m_vIDs;
  uint m_unSelID;

  CComboBox m_cmbIDs;
};
