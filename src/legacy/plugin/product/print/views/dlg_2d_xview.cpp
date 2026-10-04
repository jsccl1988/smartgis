#include "stdafx.h"
#include "legacy/plugin/product/print/views/dlg_2d_xview.h"

#include "legacy/gis/feature/leftover_copy_layer.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "legacy/plugin/product/print/shell/map_print.h"
#include "legacy/ui/catalog/map/mapmgr.h"

using namespace gis;
using namespace gis;
using namespace ui;

IMPLEMENT_DYNAMIC(CDlg2DXView, CDialog)

CDlg2DXView::CDlg2DXView(CWnd* pParent /*=NULL*/)
    : CDialog(CDlg2DXView::IDD, pParent), m_p2DXView(NULL) {}

CDlg2DXView::~CDlg2DXView() { SMT_SAFE_DELETE(m_p2DXView); }

void CDlg2DXView::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlg2DXView, CDialog)
ON_WM_DESTROY()
ON_BN_CLICKED(IDC_BTN_SAVE, &CDlg2DXView::OnBnClickedBtnSave)
END_MESSAGE_MAP()

BOOL CDlg2DXView::OnInitDialog() {
  CDialog::OnInitDialog();

  if (!InitGreateXView()) return FALSE;

  return TRUE;  // return TRUE unless you set the focus to a control
}

void CDlg2DXView::OnDestroy() {
  CDialog::OnDestroy();

  // m_p2DXView->UnbindWind();
}

BOOL CDlg2DXView::InitGreateXView(void) {
  m_p2DXView = new Smt2DXView();

  if (m_p2DXView->BindDlgItem(this, IDC_PRINT_XVIEW_CONTAINER) !=
      SMT_ERR_NONE) {
    SMT_SAFE_DELETE(m_p2DXView);
    return FALSE;
  }

  if (m_p2DXView->GetSafeHwnd())
    m_p2DXView->OnInitialUpdate();
  else {
    SMT_SAFE_DELETE(m_p2DXView);
    return FALSE;
  }

  SmtMapMgr* map_mgr = SmtMapMgr::get_singleton_ptr();
  if (!map_mgr || !map_mgr->GetSmtMapPtr()) {
    return TRUE;  // Preview HWND is up; map bind is optional.
  }
  m_p2DXView->SetOperMap(map_mgr->GetSmtMapPtr());

  return TRUE;
}

void CDlg2DXView::OnBnClickedBtnSave() {
  if (NULL != m_p2DXView) {
    LPRENDERDEVICE pRenderDevice = m_p2DXView->GetRenderDevice();
    if (NULL != pRenderDevice) {
      static char BASED_CODE szFilter[] =
          "bmp Files (*.bmp)|*.bmp|\
												gif Files (*.gif)|*.gif|\
												jpg Files (*.jpg)|*.jpg|\
												ico Files (*.ico)|*.ico|\
												png Files (*.png)|*.png|\
												mng Files (*.mng)|*.mng|\
												tif Files (*.tif)|*.tif|\
												tga Files (*.tga)|*.tga|\
												pcx Files (*.pcx)|*.pcx|\
												wbmp Files (*.wbmp)|*.wbmp|\
												wmf Files (*.wmf)|*.wmf|\
												jpc Files (*.jpc)|*.jpc|\
												jp2 Files (*.jp2)|*.jp2|\
												pgx Files (*.pgx)|*.pgx|\
												pnm Files (*.pnm)|*.pnm|\
												ras Files (*.ras)|*.ras|";

      CFileDialog dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                      szFilter, NULL);

      if (dlg.DoModal() != IDCANCEL && !dlg.GetPathName().IsEmpty()) {
        pRenderDevice->SaveImage(dlg.GetPathName());
      }
    }
  }
}