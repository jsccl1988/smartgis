
#include "stdafx.h"
#include "legacy/plugin/product/proj/views/dlg_map_prj_do_grid.h"

#include <math.h>

#include <fstream>
#include <iomanip>

#include <cstdio>
#include <string>

#include "gis/geo/proj/coordinate_transform.h"
#include "gis/geo/proj/coordinate_transform.h"
#include "legacy/gis/feature/model_aliases.h"
#include "legacy/gis/layer/layer.h"
#include "gis/map/map.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/core/util/path.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/plugin/runtime/bridge/cmd.h"
#include "legacy/plugin/runtime/auxmodule/plugin_msg.h"
#include "legacy/plugin/product/proj/shell/map_project.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_msg.h"
#include "legacy/ui/catalog/map/mapmgr.h"
using namespace gis;
using namespace geo;
using namespace sys;
using namespace ui;

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

bool fill_gauss_grid(plugin::detail::OrthoLattice& lattice, double lat_min,
                     double lat_max, double lon_min, double lon_max,
                     double d_lat, double d_lon, long scale_ruler) {
  if (d_lat == 0.0 || d_lon == 0.0) {
    return false;
  }

  const double lon_0 = gauss_kruger_central_meridian((lon_max + lon_min) * 0.5);
  geo::CoordinateTransform pipeline(iugg1975_longlat(), iugg1975_tmerc(lon_0));
  if (!pipeline.is_valid()) {
    return false;
  }

  const int n_row = static_cast<int>(fabs((lat_max - lat_min) / d_lat)) + 1;
  const int n_col = static_cast<int>(fabs((lon_max - lon_min) / d_lon)) + 1;
  lattice.ortho_resize(n_col, n_row);

  const double scale =
      (scale_ruler > 0) ? static_cast<double>(scale_ruler) : 1.0;
  for (int i = 0; i < n_row; ++i) {
    for (int j = 0; j < n_col; ++j) {
      double x = j * d_lon + lon_min;
      double y = i * d_lat + lat_min;
      if (!pipeline.transform_xy(x, y)) {
        return false;
      }
      lattice.ortho_set_point(j, i, x / scale, y / scale);
    }
  }
  return true;
}

}  // namespace

IMPLEMENT_DYNAMIC(CDlgMapPrjDoGrid, CDialog)

CDlgMapPrjDoGrid::CDlgMapPrjDoGrid(CWnd *pParent /*=NULL*/)
    : CDialog(CDlgMapPrjDoGrid::IDD, pParent),
      m_fDL(1 / 8.),
      m_fDB(1 / 12.),
      m_fLmin(117.),
      m_fBmin(36.),
      m_fLmax(118.),
      m_fBmax(37.),
      m_lScaleRuler(10000) {}

CDlgMapPrjDoGrid::~CDlgMapPrjDoGrid() {}

long CDlgMapPrjDoGrid::ScaleRuler() {
  if (GetSafeHwnd()) UpdateData(TRUE);
  return m_lScaleRuler;
}

void CDlgMapPrjDoGrid::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_DL, m_fDL);
  DDX_Text(pDX, IDC_EDIT_DB, m_fDB);

  DDX_Text(pDX, IDC_EDIT_LMIN, m_fLmin);
  DDX_Text(pDX, IDC_EDIT_BMIN, m_fBmin);
  DDX_Text(pDX, IDC_EDIT_LMAX, m_fLmax);
  DDX_Text(pDX, IDC_EDIT_BMAX, m_fBmax);

  DDX_Text(pDX, IDC_EDIT_SCALE, m_lScaleRuler);
}

BEGIN_MESSAGE_MAP(CDlgMapPrjDoGrid, CDialog)
ON_WM_CTLCOLOR()
ON_BN_CLICKED(IDC_BTN_DOGRID, &CDlgMapPrjDoGrid::OnBnClickedBtnDogrid)
END_MESSAGE_MAP()

void CDlgMapPrjDoGrid::OutputRes(plugin::detail::OrthoLattice& lattice) {
  string strAppTempPath = get_app_temp_path();
  strAppTempPath += "GridRes.txt";
  fstream fOut;
  fOut.open(strAppTempPath.c_str(), ios::out);
  if (fOut.is_open()) {
    const int nM = lattice.ny;
    const int nN = lattice.nx;

    fOut << "minL:" << m_fLmin << "   maxL:" << m_fLmax << endl;
    fOut << "minB:" << m_fBmin << "   maxB:" << m_fBmax << endl;
    fOut << "Scale Ruler:  1:" << m_lScaleRuler << endl;
    fOut << "Grid Size:" << nN << "   " << nM << endl;

    double l, b;

    for (int j = 0; j < nN; j++) {
      l = m_fLmin + j * m_fDL;
      fOut << setprecision(4) << "\t" << l << "\t";
    }
    fOut << endl;

    for (int i = nM - 1; i > -1; i--) {
      b = m_fBmin + i * m_fDB;
      fOut << setprecision(4) << b;
      for (int j = 0; j < nN; j++) {
        double px = 0;
        double py = 0;
        lattice.ortho_point(j, i, &px, &py);
        fOut << setprecision(4) << "\t(" << px << "," << py << ")";
      }
      fOut << endl;
    }
    fOut.close();
  }
}

void CDlgMapPrjDoGrid::OnBnClickedBtnDogrid() {
  UpdateData(TRUE);

  SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
  Layer *pLayer = pSmtMapMgr->GetActiveLayer();

  if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) return;

  SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;

  if (pVLayer && leftover_layer_feature_type(pVLayer) == FtGrid) {
    SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    FeatureAdapter *pSmtFeature = new FeatureAdapter;
    plugin::detail::OrthoLattice lattice;

    if (!fill_gauss_grid(lattice, m_fBmin, m_fBmax, m_fLmin, m_fLmax, m_fDB,
                         m_fDL, m_lScaleRuler)) {
      SMT_SAFE_DELETE(pSmtFeature);
      ::MessageBox(::GetActiveWindow(), "投影失败!", "提示", MB_OK);
      return;
    }
    OutputRes(lattice);

    pSmtFeature->SetFeatureType(FeatureType::FtGrid);
    pSmtFeature->SetStyle(styleSonfig.szPointStyle);
    pSmtFeature->SetGeometry(&lattice.nodes);

    if (pSmtMapMgr->AppendFeature(pSmtFeature, false)) {
      SmtListenerMsg param;
      param.hSrcWnd = m_hWnd;
      ::MessageBox(::GetActiveWindow(), "生成成功!", "提示", MB_OK);
      (void)plugin::command_id_from_am_msg(GT_MSG_VIEW_ZOOMREFRESH);
      post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST, GT_MSG_VIEW_ZOOMREFRESH,
                       param);
    } else
      SMT_SAFE_DELETE(pSmtFeature);
  } else
    ::MessageBox(::GetActiveWindow(), "请激活GRID图层!", "提示", MB_OK);
}
