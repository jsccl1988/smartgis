#pragma once

#include "legacy/plugin/dem/resource.h"


class CDlgAbout : public CDialog {
  DECLARE_DYNAMIC(CDlgAbout)

 public:
  CDlgAbout(CWnd* pParent = NULL);
  virtual ~CDlgAbout();

  enum { IDD = IDD_ABOUTBOX };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedOk();
};
