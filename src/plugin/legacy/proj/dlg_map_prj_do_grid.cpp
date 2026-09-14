// dlg_map_prj_do_grid.cpp : implementation file

#include "stdafx.h"
#include "plugin/legacy/proj/map_project.h"
#include "plugin/legacy/proj/dlg_map_prj_do_grid.h"

#include <math.h>

#include <fstream>
#include <iomanip>

#include "algorithm/geo/geometry.h"
#include "algorithm/proj/projection.h"
#include "base/core/api.h"
#include "base/core/msg.h"
#include "base/style/stylemanager.h"
#include "plugin/host/legacy_cmd.h"
#include "plugin/legacy/plugin_msg.h"
#include "sdb/feature/feature.h"
#include "sdb/layer/layer.h"
#include "sdb/map/map.h"
#include "sys/sysmanager.h"
#include "legacy_tool/group/defs.h"
#include "legacy_tool/t_msg.h"
#include "legacy_ui/xcatalog/mapmgr.h"

using namespace base;
using namespace sdb;
using namespace geo;
using namespace geo;
using namespace sys;
using namespace ui;

namespace {

bool fill_gauss_grid(SmtGrid &grid, double lat_min, double lat_max,
                     double lon_min, double lon_max, double d_lat, double d_lon,
                     long scale_ruler) {
  if (d_lat == 0.0 || d_lon == 0.0) {
    return false;
  }

  SmtProjection geo = {};
  SmtProjection gk = {};
  const double lon_0 = gauss_kruger_central_meridian((lon_max + lon_min) * 0.5);
  if (init_projection(&geo) != SMT_ERR_NONE ||
      init_projection(&gk) != SMT_ERR_NONE ||
      load_longlat_ellipsoid(&geo, kIugg1975A, kIugg1975B) != SMT_ERR_NONE ||
      load_tmerc_crs(&gk, kIugg1975A, kIugg1975B, lon_0) != SMT_ERR_NONE) {
    free_projection(&geo);
    free_projection(&gk);
    return false;
  }

  const int n_row = static_cast<int>(fabs((lat_max - lat_min) / d_lat)) + 1;
  const int n_col = static_cast<int>(fabs((lon_max - lon_min) / d_lon)) + 1;
  grid.resize(n_row, n_col);

  const double scale =
      (scale_ruler > 0) ? static_cast<double>(scale_ruler) : 1.0;
  bool ok = true;
  for (int i = 0; i < n_row && ok; ++i) {
    for (int j = 0; j < n_col; ++j) {
      dbfPoint point(j * d_lon + lon_min, i * d_lat + lat_min);
      if (project_point(&geo, &gk, &point) != SMT_ERR_NONE) {
        ok = false;
        break;
      }
      grid.set_node(i, j, RawPoint(point.x / scale, point.y / scale));
    }
  }

  free_projection(&geo);
  free_projection(&gk);
  return ok;
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

void CDlgMapPrjDoGrid::OutputRes(SmtGrid &grid) {
  string strAppTempPath = get_app_temp_path();
  strAppTempPath += "GridRes.txt";
  fstream fOut;
  fOut.open(strAppTempPath.c_str(), ios::out);
  if (fOut.is_open()) {
    int nN, nM;
    grid.get_size(nM, nN);

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
        RawPoint rawPt = grid.node(i, j);
        fOut << setprecision(4) << "\t(" << rawPt.x << "," << rawPt.y << ")";
      }
      fOut << endl;
    }
    fOut.close();
  }
}

void CDlgMapPrjDoGrid::OnBnClickedBtnDogrid() {
  UpdateData(TRUE);

  SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
  SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

  if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) return;

  SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;

  if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtGrid) {
    SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    SmtFeature *pSmtFeature = new SmtFeature;
    SmtGrid oSmtGrid;

    if (!fill_gauss_grid(oSmtGrid, m_fBmin, m_fBmax, m_fLmin, m_fLmax, m_fDB,
                         m_fDL, m_lScaleRuler)) {
      SMT_SAFE_DELETE(pSmtFeature);
      ::MessageBox(::GetActiveWindow(), "ͶӰʧ��!", "��ʾ", MB_OK);
      return;
    }
    OutputRes(oSmtGrid);

    pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
    pSmtFeature->SetStyle(styleSonfig.szPointStyle);
    pSmtFeature->SetGeometry(&oSmtGrid);

    if (pSmtMapMgr->AppendFeature(pSmtFeature, false)) {
      SmtListenerMsg param;
      param.hSrcWnd = m_hWnd;
      ::MessageBox(::GetActiveWindow(), "���ɳɹ�!", "��ʾ", MB_OK);
      (void)plugin::command_id_from_am_msg(GT_MSG_VIEW_ZOOMREFRESH);
      post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST, GT_MSG_VIEW_ZOOMREFRESH,
                       param);
    } else
      SMT_SAFE_DELETE(pSmtFeature);
  } else
    ::MessageBox(::GetActiveWindow(), "�뼤��GRIDͼ��!", "��ʾ", MB_OK);
}
