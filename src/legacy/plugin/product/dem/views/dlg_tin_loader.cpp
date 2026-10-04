
#include "stdafx.h"
#include "legacy/plugin/product/dem/views/dlg_tin_loader.h"

#include "gis/geo/ops/geometry_traits.h"
#include "legacy/core/util/string.h"
#include "legacy/plugin/product/dem/shell/dem_creater.h"
#include "legacy/plugin/product/dem/shell/dem_dlg_helpers.h"
#include "legacy/render/scene3d/primitive/surface/terrain.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/ui/catalog/map/mapmgr.h"
#include "legacy/ui/catalog/scene/scenemgr.h"
#include "legacy/gis/layer/layer.h"
#include "plugin/product/world3d/grid/dem/loader/trimesh_loader.h"
using namespace gis;
using namespace plugin;
using namespace sys;
using namespace plugin;
using namespace ui;

IMPLEMENT_DYNAMIC(CDlgTinLoader, CDialog)

CDlgTinLoader::CDlgTinLoader(CWnd *pParent /*=NULL*/)
    : CDialog(CDlgTinLoader::IDD, pParent),
      m_p3DView(NULL),
      m_p3DRenderDevice(NULL),
      m_pScene(NULL),
      m_strVertexUrl(_T("")),
      m_fXScale(0.05),
      m_fYScale(0.05),
      m_fZScale(0.05),
      m_iX(0),
      m_iY(0),
      m_iZ(0),
      m_strTexUrl(_T("")),
      m_nHeadSkip(1),
      m_nLineSkip(4),
      m_nCol(0),
      m_nSeparator(ST_COMMA) {}

CDlgTinLoader::~CDlgTinLoader() {
  m_p3DView = NULL;
  m_p3DRenderDevice = NULL;
  m_pScene = NULL;
}

void CDlgTinLoader::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_GRID_COLV, m_colvGrid);
  DDX_Text(pDX, IDC_EDIT_VERTEX_URL, m_strVertexUrl);
  DDX_Control(pDX, IDC_CMB_X, m_cmbX);
  DDX_Control(pDX, IDC_CMB_Y, m_cmbY);
  DDX_Control(pDX, IDC_CMB_Z, m_cmbZ);
  DDX_Text(pDX, IDC_EDIT_XSCALE, m_fXScale);
  DDX_Text(pDX, IDC_EDIT_YSCALE, m_fYScale);
  DDX_Text(pDX, IDC_EDIT_ZSCALE, m_fZScale);
  DDX_Text(pDX, IDC_EDIT_TEX_URL, m_strTexUrl);
  DDX_Control(pDX, IDC_CMB_CLRTYPE, m_cmbClrType);
  DDX_Control(pDX, IDC_CMB_SELTINLAYER, m_cmbTinLayer);
  DDX_Text(pDX, IDC_EDIT_HEADSKIP, m_nHeadSkip);
  DDX_Text(pDX, IDC_EDIT_LINESKIP, m_nLineSkip);
}

BEGIN_MESSAGE_MAP(CDlgTinLoader, CDialog)
ON_BN_CLICKED(IDC_BTN_SELVERTEXFILE,
              &CDlgTinLoader::OnBnClickedBtnSelvertexfile)
ON_BN_CLICKED(IDC_BTN_SELTEXFILE, &CDlgTinLoader::OnBnClickedBtnSeltexfile)
ON_BN_CLICKED(IDOK, &CDlgTinLoader::OnBnClickedOk)
ON_BN_CLICKED(IDC_RADIO_TAB, &CDlgTinLoader::OnBnClickedRadioTab)
ON_BN_CLICKED(IDC_RADIO_SPACE, &CDlgTinLoader::OnBnClickedRadioSpace)
ON_BN_CLICKED(IDC_RADIO_COMMA, &CDlgTinLoader::OnBnClickedRadioComma)
ON_BN_CLICKED(IDC_CK_USETEX, &CDlgTinLoader::OnBnClickedCkUsetex)
ON_BN_CLICKED(IDC_CK_GEN2DTIN, &CDlgTinLoader::OnBnClickedCkGen2DTin)
ON_EN_CHANGE(IDC_EDIT_XSCALE, &CDlgTinLoader::OnEnChangeEditXscale)
ON_EN_CHANGE(IDC_EDIT_YSCALE, &CDlgTinLoader::OnEnChangeEditYscale)
ON_EN_CHANGE(IDC_EDIT_ZSCALE, &CDlgTinLoader::OnEnChangeEditZscale)
END_MESSAGE_MAP()

BOOL CDlgTinLoader::OnInitDialog() {
  CDialog::OnInitDialog();

  if (SMT_ERR_NONE != Init3DStuff()) return FALSE;

  m_colvGrid.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  CButton *pRadioSepa = (CButton *)GetDlgItem(IDC_RADIO_COMMA);
  if (pRadioSepa) {
    pRadioSepa->SetCheck(1);
    m_nSeparator = ST_COMMA;
  }

  dem_dlg::disable_tex_controls(this, &m_bUseTex);

  CButton *pCkGen2DTin = (CButton *)GetDlgItem(IDC_CK_GEN2DTIN);
  if (pCkGen2DTin) {
    pCkGen2DTin->SetCheck(0);
    m_cmbTinLayer.EnableWindow(FALSE);
    m_bGen2DTin = false;
  }
  UpdateClrTypeCmb();

  Update2DTinLayerCmb();

  UpdateData(FALSE);

  return TRUE;  // return TRUE unless you set the focus to a control
}
int CDlgTinLoader::Init3DStuff(void) {
  return dem_dlg::bind_sys_3d_view(m_p3DView, m_p3DRenderDevice, m_pScene);
}

void CDlgTinLoader::OnBnClickedBtnSelvertexfile() {
  static char BASED_CODE szFilter[] =
      "Data Files (*.dat)|*.dat|Txt Files (*.txt)|*.txt|All Files (*.*)|*.*||";

  CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  szFilter, NULL);

  if (dlg.DoModal() == IDCANCEL) {
    return;
  }

  m_strVertexUrl = dlg.GetPathName();

  OnSelVertexFile();

  UpdateData(FALSE);
}

void CDlgTinLoader::OnBnClickedBtnSeltexfile() {
  if (dem_dlg::pick_bmp_texture(&m_strTexUrl, &m_strTexDir, &m_strTexName,
                                &m_strTexExt)) {
    UpdateData(FALSE);
  }
}

void CDlgTinLoader::OnBnClickedRadioTab() {
  m_nSeparator = ST_TAB;
  CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
  CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
  CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

  if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma) {
    // pRadioSepaTab->SetCheck(0);
    pRadioSepaSpace->SetCheck(0);
    pRadioSepaComma->SetCheck(0);
  }

  OnChangeSeparator();
}

void CDlgTinLoader::OnBnClickedRadioSpace() {
  m_nSeparator = ST_SPACE;

  CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
  CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
  CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

  if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma) {
    pRadioSepaTab->SetCheck(0);
    // pRadioSepaSpace->SetCheck(0);
    pRadioSepaComma->SetCheck(0);
  }

  OnChangeSeparator();
}

void CDlgTinLoader::OnBnClickedRadioComma() {
  m_nSeparator = ST_COMMA;

  CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
  CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
  CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

  if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma) {
    pRadioSepaTab->SetCheck(0);
    pRadioSepaSpace->SetCheck(0);
    // pRadioSepaComma->SetCheck(0);
  }
  OnChangeSeparator();
}

void CDlgTinLoader::OnChangeSeparator(void) {
  vector<string> vFldVals;
  if (m_vTextBuf.size() > 0) {
    SplitString(vFldVals, m_vTextBuf[0]);

    m_nCol = vFldVals.size();

    if (m_nCol < 3) {
      m_cmbX.EnableWindow(FALSE);
      m_cmbY.EnableWindow(FALSE);
      m_cmbZ.EnableWindow(FALSE);
    } else {
      m_cmbX.EnableWindow(TRUE);
      m_cmbY.EnableWindow(TRUE);
      m_cmbZ.EnableWindow(TRUE);

      UpdateXYZCmb();
    }

    UpdateColVGrid();
  }
}

void CDlgTinLoader::OnBnClickedCkUsetex() {
  dem_dlg::apply_usetex_toggle(this, &m_bUseTex);
}

void CDlgTinLoader::OnBnClickedCkGen2DTin() {
  CButton *pCkGen2DTin = (CButton *)GetDlgItem(IDC_CK_GEN2DTIN);
  if (pCkGen2DTin) {
    m_bGen2DTin = (pCkGen2DTin->GetCheck() == 1);
    m_cmbTinLayer.EnableWindow(m_bGen2DTin);
  }
}

void CDlgTinLoader::OnSelVertexFile(void) {
  if (m_strVertexUrl == "") return;

  fstream fin;
  locale loc1 = locale::global(locale(".936"));
  fin.open(m_strVertexUrl, ios::in);
  locale::global(locale(loc1));

  if (!fin.is_open()) return;

  int nCount = 3;
  char szBuf[2000];

  m_vTextBuf.clear();
  while (!fin.eof() && (--nCount) > 0) {
    fin.getline(szBuf, 2000, '\n');
    m_vTextBuf.push_back(szBuf);
  }

  fin.close();

  m_nCol = 1;

  OnChangeSeparator();
  UpdateColVGrid();
}

void CDlgTinLoader::UpdateColVGrid(void) {
  UpdateColVGridHead();
  UpdateColVGridContent();
  m_colvGrid.Invalidate();
}

void CDlgTinLoader::UpdateColVGridHead(void) {
  m_colvGrid.DeleteAllItems();
  while (m_colvGrid.DeleteColumn(0)) {
  }

  m_colvGrid.InsertColumn(0, _T("No."), LVCFMT_LEFT, 40);
  for (int i = 0; i < m_nCol; i++) {
    CString title;
    title.Format(_T("Col %d"), i + 1);
    m_colvGrid.InsertColumn(i + 1, title, LVCFMT_LEFT, 60);
  }
}

void CDlgTinLoader::UpdateColVGridContent(void) {
  m_colvGrid.DeleteAllItems();

  vector<string> vFldVals;
  for (int i = 0; i < static_cast<int>(m_vTextBuf.size()); i++) {
    CString row_label;
    row_label.Format(_T("Row %d"), i + 1);
    const int row = m_colvGrid.InsertItem(i, row_label);

    vFldVals.clear();
    SplitString(vFldVals, m_vTextBuf[i]);

    for (int j = 0; j < static_cast<int>(vFldVals.size()); j++) {
      m_colvGrid.SetItemText(row, j + 1, vFldVals[j].c_str());
    }
  }
}

void CDlgTinLoader::UpdateClrTypeCmb(void) {
  m_cmbClrType.ResetContent();
  m_cmbClrType.AddString("white");
  m_cmbClrType.AddString("grade");
  m_cmbClrType.AddString("color");
  m_cmbClrType.SetCurSel(1);
}

void CDlgTinLoader::UpdateXYZCmb(void) {
  CString strColName = "";
  m_cmbX.ResetContent();
  m_cmbY.ResetContent();
  m_cmbZ.ResetContent();
  for (int i = 0; i < m_nCol; ++i) {
    strColName.Format("Col %d", i + 1);
    m_cmbX.AddString(strColName);
    m_cmbY.AddString(strColName);
    m_cmbZ.AddString(strColName);
  }

  m_cmbX.SetCurSel(0);
  m_cmbY.SetCurSel(1);
  m_cmbZ.SetCurSel(2);
}

void CDlgTinLoader::Update2DTinLayerCmb(void) {
  SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
  m_cmbTinLayer.ResetContent();
  Map *pMap = pSmtMapMgr->GetSmtMapPtr();
  for (int i = 0; i < pMap->GetLayerCount(); i++) {
    Layer *pLayer = pSmtMapMgr->GetLayer(i);

    if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) continue;

    SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
    if (pVLayer && leftover_layer_feature_type(pVLayer) == FtTin) {
      m_cmbTinLayer.AddString(pLayer->GetLayerName());
    }
  }
}

void CDlgTinLoader::OnEnChangeEditXscale() { UpdateData(TRUE); }

void CDlgTinLoader::OnEnChangeEditYscale() { UpdateData(TRUE); }

void CDlgTinLoader::OnEnChangeEditZscale() { UpdateData(TRUE); }

void CDlgTinLoader::OnBnClickedOk() {
  UpdateData(TRUE);
  CString strClrType = "";
  int nClrType = m_cmbClrType.GetCurSel() + 1;

  CString strIX = "", strIY = "", strIZ = "";
  m_cmbX.GetLBText(m_cmbX.GetCurSel(), strIX);
  m_cmbY.GetLBText(m_cmbY.GetCurSel(), strIY);
  m_cmbZ.GetLBText(m_cmbZ.GetCurSel(), strIZ);
  sscanf(strIX, "第%d列", &m_iX);
  sscanf(strIY, "第%d列", &m_iY);
  sscanf(strIZ, "第%d列", &m_iZ);

  m_iX--;
  m_iY--;
  m_iZ--;

  if (m_iX == m_iY) {
    ::MessageBox(::GetActiveWindow(), "XY选择不能相同!", "提示", MB_OK);
    return;
  }

  if (m_bUseTex && m_strTexDir != "" && m_strTexName != "") {
    string strTexturePath = "";
    SmtTexture *pTexture = NULL;

    strTexturePath = m_strTexDir + m_strTexName + m_strTexExt;
    pTexture = m_p3DRenderDevice->CreateTexture((LPCTSTR)m_strTexName);
    if (NULL != pTexture) pTexture->Load(strTexturePath.c_str());
  }

  SmtMaterial matMaterial;
  matMaterial.SetAmbientValue(SmtColor(0.2, 0.2, 0.2, 1.0));
  matMaterial.SetDiffuseValue(SmtColor(0.8, 0.8, 0.8, 1.0));
  matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 1.0));
  matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.0));
  matMaterial.SetShininessValue(0);

  SmtTerrain *pTerrain = new SmtTerrain;

  pTerrain->SetClrType(nClrType);
  pTerrain->SetXScale(m_fXScale);
  pTerrain->SetYScale(m_fYScale);
  pTerrain->SetZScale(m_fZScale);

  TrimeshFileFmt tfFmt;
  tfFmt.iX = m_iX;
  tfFmt.iY = m_iY;
  tfFmt.iZ = m_iZ;
  tfFmt.nCol = m_nCol;
  tfFmt.nHeadSkip = m_nHeadSkip;
  tfFmt.nLineSkip = m_nLineSkip;
  tfFmt.nSeparatorType = m_nSeparator;

  OGRTriangulatedSurface tin_surf;
  SmtSceneMgr *pSceneMgr = SmtSceneMgr::get_singleton_ptr();
  Vector3 pos(30, 30, 30);

  if (SMT_ERR_NONE == load_ascii_xyz_trimesh(m_strVertexUrl, tfFmt, m_fXScale,
                                         m_fYScale, m_fZScale, &tin_surf) &&
      SMT_ERR_NONE == pTerrain->Init(pos, matMaterial, m_strTexName) &&
      SMT_ERR_NONE == pTerrain->SetTerrainSurf(&tin_surf) &&
      SMT_ERR_NONE == pTerrain->Create(m_p3DRenderDevice)) {
    pSceneMgr->Add3DObject(pTerrain);
  }

  if (m_bGen2DTin) {
    CString strTinLayer = "";
    m_cmbTinLayer.GetLBText(m_cmbTinLayer.GetCurSel(), strTinLayer);
    if (strTinLayer != "") {
      SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();

      Layer *pLayer = pSmtMapMgr->GetLayer(strTinLayer);

      if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) return;

      SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;

      if (pVLayer && leftover_layer_feature_type(pVLayer) == FtTin) {
        OGRTriangulatedSurface *pTinSurf = pTerrain->GetTerrainSurf();
        if (pTinSurf) {
          SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
          SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

          FeatureAdapter *pSmtFeature = new FeatureAdapter;

          pSmtFeature->SetFeatureType(FeatureType::FtTin);
          pSmtFeature->SetStyle(styleSonfig.szPointStyle);
          pSmtFeature->SetGeometry(pTinSurf);

          if (!pSmtMapMgr->AppendFeature(pSmtFeature, false))
            SMT_SAFE_DELETE(pSmtFeature);
        }
      }
    }
  }

  OnOK();
}

long CDlgTinLoader::SplitString(vector<string> &vFldVal, string strContent) {
  switch (m_nSeparator) {
    case ST_TAB:
      str_tokenize(strContent, vFldVal, "\t");
      break;
    case ST_SPACE:
      str_tokenize(strContent, vFldVal, " ");
      break;
    case ST_COMMA:
      str_tokenize(strContent, vFldVal, ",");
      break;
  }

  return SMT_ERR_NONE;
}