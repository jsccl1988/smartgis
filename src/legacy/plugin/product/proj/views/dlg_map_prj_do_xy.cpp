
#include "stdafx.h"
#include "legacy/plugin/product/proj/views/dlg_map_prj_do_xy.h"

#include <cstdio>
#include <string>

#include "gis/geo/proj/coordinate_transform.h"
#include "legacy/plugin/product/proj/shell/map_project.h"

namespace {

constexpr double kIugg1975A = 6378140.0;
constexpr double kIugg1975B = 6356755.2882;

double gauss_kruger_central_meridian(double lon_deg) {
  double lon = lon_deg;
  while (lon < 0.0) {
    lon += 360.0;
  }
  while (lon >= 360.0) {
    lon -= 360.0;
  }
  const int zone = static_cast<int>(lon / 6.0) + 1;
  return static_cast<double>(zone) * 6.0 - 3.0;
}

std::string iugg1975_longlat() {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "+proj=longlat +a=%.10f +b=%.10f +type=crs",
                kIugg1975A, kIugg1975B);
  return buf;
}

std::string iugg1975_tmerc(double lon_0) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "+proj=tmerc +lat_0=0 +lon_0=%.8f +k=1 +x_0=500000 +y_0=0 "
                "+a=%.10f +b=%.10f +units=m +type=crs",
                lon_0, kIugg1975A, kIugg1975B);
  return buf;
}

}  // namespace

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

  geo::CoordinateTransform pipeline(
      iugg1975_longlat(),
      iugg1975_tmerc(gauss_kruger_central_meridian(m_fL)));
  double x = m_fL;
  double y = m_fB;
  if (pipeline.transform_xy(x, y)) {
    const double scale =
        (m_lScaleRuler > 0) ? static_cast<double>(m_lScaleRuler) : 1.0;
    m_fX = x / scale;
    m_fY = y / scale;
    UpdateData(FALSE);
  }
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
