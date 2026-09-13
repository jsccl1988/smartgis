// dlg_map_prj_do_xy.cpp : implementation file

#include "stdafx.h"
#include "plugin/legacy/proj/map_project.h"
#include "plugin/legacy/proj/dlg_map_prj_do_xy.h"

#include "algorithm/proj/projection.h"

using namespace base;
using namespace geo;

IMPLEMENT_DYNAMIC(CDlgMapPrjDoXY, CDialog)

CDlgMapPrjDoXY::CDlgMapPrjDoXY(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgMapPrjDoXY::IDD, pParent),
      m_fL(120),
      m_fB(36),
      m_fX(0),
      m_fY(0),
      m_lScaleRuler(1) {}

CDlgMapPrjDoXY::~CDlgMapPrjDoXY() {}

void CDlgMapPrjDoXY::SetScaleRuler(long scale_ruler) {
  m_lScaleRuler = (scale_ruler > 0) ? scale_ruler : 1;
  if (GetSafeHwnd()) refresh_scale_label();
}

void CDlgMapPrjDoXY::refresh_scale_label() {
  CString str;
  str.Format("1:%d", m_lScaleRuler);
  GetDlgItem(IDC_STATIC_SCALERULER)->SetWindowText(str);
}

void CDlgMapPrjDoXY::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_L, m_fL);
  DDX_Text(pDX, IDC_EDIT_B, m_fB);
  DDX_Text(pDX, IDC_EDIT_X, m_fX);
  DDX_Text(pDX, IDC_EDIT_Y, m_fY);
}

BEGIN_MESSAGE_MAP(CDlgMapPrjDoXY, CDialog)
ON_BN_CLICKED(IDC_BTN_DOXY, &CDlgMapPrjDoXY::OnBnClickedBtnDoxy)
ON_WM_CTLCOLOR()
ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

void CDlgMapPrjDoXY::OnBnClickedBtnDoxy() {
  UpdateData(TRUE);

  SmtProjection geo = {};
  SmtProjection gk = {};
  if (init_projection(&geo) != SMT_ERR_NONE ||
      init_projection(&gk) != SMT_ERR_NONE ||
      load_longlat_ellipsoid(&geo, kIugg1975A, kIugg1975B) != SMT_ERR_NONE ||
      load_tmerc_crs(&gk, kIugg1975A, kIugg1975B,
                     gauss_kruger_central_meridian(m_fL)) != SMT_ERR_NONE) {
    free_projection(&geo);
    free_projection(&gk);
    return;
  }

  dbfPoint point(m_fL, m_fB);
  if (project_point(&geo, &gk, &point) == SMT_ERR_NONE) {
    const double scale =
        (m_lScaleRuler > 0) ? static_cast<double>(m_lScaleRuler) : 1.0;
    m_fX = point.x / scale;
    m_fY = point.y / scale;
    UpdateData(FALSE);
  }

  free_projection(&geo);
  free_projection(&gk);
}

BOOL CDlgMapPrjDoXY::OnInitDialog() {
  CDialog::OnInitDialog();
  refresh_scale_label();
  return TRUE;
}

void CDlgMapPrjDoXY::OnShowWindow(BOOL bShow, UINT nStatus) {
  CDialog::OnShowWindow(bShow, nStatus);
  if (bShow) refresh_scale_label();
}
