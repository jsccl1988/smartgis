
#include "stdafx.h"
#include "legacy/plugin/product/dem/views/dlg_about.h"

#include "legacy/plugin/product/dem/shell/dem_creater.h"
IMPLEMENT_DYNAMIC(CDlgAbout, CDialog)

CDlgAbout::CDlgAbout(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgAbout::IDD, pParent) {}

CDlgAbout::~CDlgAbout() {}

void CDlgAbout::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgAbout, CDialog)
ON_BN_CLICKED(IDOK, &CDlgAbout::OnBnClickedOk)
END_MESSAGE_MAP()

void CDlgAbout::OnBnClickedOk() { OnOK(); }
