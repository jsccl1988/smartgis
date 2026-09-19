// SmtDSXCatalog.cpp : 瀹炵幇鏂囦欢
//

#include "stdafx.h"
#include "dsxcatalog.h"

#include "base/core/api.h"
#include "base/core/log.h"
#include "legacy/ui/xcatalog/mapmgr.h"
#include "legacy/ui/xcatalog/xcatalog_core.h"
#include "gis/datasource/mgr/datasource_mgr.h"
#include "gis/feature/feature_api.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"

// using namespace Smt_GIS;
// using namespace Smt_SDEDevMgr;
// using namespace Smt_Sys;

#include "legacy/ui/xcatalog/dlg_create_ds.h"
#include "legacy/ui/xcatalog/dlg_create_layer.h"

// SmtDSXCatalog

namespace ui {
IMPLEMENT_DYNAMIC(SmtDSXCatalog, SmtXCatalog)

SmtDSXCatalog::SmtDSXCatalog() {
  m_hContexMenu = NULL;
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();
  SmtDataSource pDS = pDSMgr->get_active_data_source();
  if (pDS) {
    m_strSelDSName = pDS.GetName();
  }
}

SmtDSXCatalog::~SmtDSXCatalog() {}

BEGIN_MESSAGE_MAP(SmtDSXCatalog, SmtXCatalog)
ON_WM_CREATE()
ON_WM_RBUTTONDOWN()

ON_COMMAND(ID_DS_LAYER_CREATE, &SmtDSXCatalog::OnDsLayerCreate)
ON_COMMAND(ID_DS_LAYER_DELETE, &SmtDSXCatalog::OnDsLayerDelete)
ON_COMMAND(ID_DS_LAYER_PROPERTY, &SmtDSXCatalog::OnDsLayerProperty)

ON_COMMAND(ID_DS_PROPERTY, &SmtDSXCatalog::OnDsProperty)
ON_COMMAND(ID_DS_LAYER_LOAD_SHP, &SmtDSXCatalog::OnDsLayerLoadShp)
ON_COMMAND(ID_DS_LAYER_LOAD_IMG, &SmtDSXCatalog::OnDsLayerLoadImage)

ON_COMMAND(ID_DS_DELETE, &SmtDSXCatalog::OnDsDelete)
ON_COMMAND(ID_SVR_DS_CREATE, &SmtDSXCatalog::OnSvrDsCreate)
ON_COMMAND(ID_SVR_DS_APPEND, &SmtDSXCatalog::OnSvrDsAppend)
ON_COMMAND(ID_DS_SET_ACTIVE, &SmtDSXCatalog::OnSvrDsSetActive)

END_MESSAGE_MAP()

// SmtDSXCatalog 娑堟伅澶勭悊绋嬪簭
bool SmtDSXCatalog::InitCreate(void) {
  // AFX_MANAGE_STATE(AfxGetStaticModuleState());

#ifdef _DEBUG
  HINSTANCE hInstance = ::GetModuleHandle("ui_legacy_d.dll");
#else
  HINSTANCE hInstance = ::GetModuleHandle("ui_legacy.dll");
#endif

  m_imgList.Create(16, 16, ILC_COLOR16 | ILC_MASK, 1, 0);

  m_imgList.SetBkColor(RGB(255, 255, 255));

  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_HOME)));
  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_DS)));
  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_LAYER)));
  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_DOC)));

  SetImageList(&m_imgList, TVSIL_NORMAL);

  return SmtXCatalog::InitCreate();
}

bool SmtDSXCatalog::EndDestory(void) { return SmtXCatalog::EndDestory(); }

bool SmtDSXCatalog::CreateContexMenu() {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  CMenu menuDSMgr;
  menuDSMgr.LoadMenu(IDR_MENU_DSMGR);
  m_hContexMenu = menuDSMgr.GetSafeHmenu();

  return SmtXCatalog::CreateContexMenu();
}

//////////////////////////////////////////////////////////////////////////
BOOL SmtDSXCatalog::Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd,
                           UINT nID) {
  // TODO: 鍦ㄦ娣诲姞涓撶敤浠ｇ爜鍜?鎴栬皟鐢ㄥ熀绫?
  return SmtXCatalog::Create(dwStyle, rect, pParentWnd, nID);
}

int SmtDSXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXCatalog::OnCreate(lpCreateStruct) == -1) return -1;

  // TODO:  鍦ㄦ娣诲姞鎮ㄤ笓鐢ㄧ殑鍒涘缓浠ｇ爜

  return 0;
}

void SmtDSXCatalog::OnRButtonDown(UINT nFlags, CPoint point) {
  // TODO:
  // 鍦ㄦ娣诲姞娑堟伅澶勭悊绋嬪簭浠ｇ爜鍜?鎴栬皟鐢ㄩ粯璁ゅ€?
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  HTREEITEM hItem = HitTest(point, &nFlags);
  if ((hItem != NULL) && (TVHT_ONITEM & nFlags)) {
    SelectItem(hItem);
  } else
    return;

  CMenu menuMapMgr;
  menuMapMgr.LoadMenu(IDR_MENU_DSMGR);

  CMenu *pMenu = NULL;
  HTREEITEM hParentItem = GetParentItem(hItem);

  if (hItem == m_hDSCatalog) {
    pMenu = menuMapMgr.GetSubMenu(2);
  } else if (hParentItem == m_hDSCatalog) {
    pMenu = menuMapMgr.GetSubMenu(1);
    m_strSelDSName = GetItemText(hItem);
  } else {
    pMenu = menuMapMgr.GetSubMenu(0);
    m_strSelDSName = GetItemText(hParentItem);
    m_strSelDSLayerName = GetItemText(hItem);
  }

  if (pMenu) {
    CPoint menuPos;
    GetCursorPos(&menuPos);
    pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                          menuPos.x, menuPos.y, this);
    menuMapMgr.Detach();
  }

  SmtXCatalog::OnRButtonDown(nFlags, point);
}

void SmtDSXCatalog::UpdateCatalogTree(void) {
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  SetRedraw(FALSE);
  DeleteAllItems();
  SetTextColor(RGB(0, 0, 255));
  m_hDSCatalog = InsertItem("Data Sources", 0, 0, TVI_ROOT);

  pDSMgr->move_first();
  while (!pDSMgr->is_end()) {
    AppendDSNode(pDSMgr->get_data_source());
    pDSMgr->move_next();
  }

  Expand(m_hDSCatalog, TVE_EXPAND);

  SetRedraw(TRUE);

  RedrawWindow();
}

void SmtDSXCatalog::AppendDSNode(SmtDataSource pDS) {
  if (pDS && pDS.Open()) {
    HTREEITEM hDS = InsertItem(pDS.GetName(), 1, 1, m_hDSCatalog);

    SmtLayerInfo layerArchiveInfo;
    int nLayers = pDS.GetLayerCount();
    for (int i = 0; i < nLayers; i++) {
      pDS.GetLayerInfo(layerArchiveInfo, i);
      HTREEITEM hLayerNode = InsertItem(layerArchiveInfo.szName, 2, 2, hDS);
      InsertItem(layerArchiveInfo.szArchiveName, 3, 3, hLayerNode);
      InsertItem(SmtDataSource::GetLayerFeatureTypeName(
                     layerArchiveInfo.unFeatureType),
                 3, 3, hLayerNode);
      // Expand(hLayerNode,TVE_EXPAND);
    }

    pDS.Close();
  }
}

//////////////////////////////////////////////////////////////////////////
void SmtDSXCatalog::OnDsLayerCreate() {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  ::SetCapture(AfxGetMainWnd()->m_hWnd);

  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  SmtDataSource pDS = pDSMgr->get_data_source(GetSelDSName());

  if (pDS && pDS.Open()) {
    SmtLayer *pLayer = NULL;
    CDlgCreateLayer dlg(this);
    if (dlg.DoModal() == IDOK) {
      UINT unFcls = dlg.GetSelFcls();
      fRect lyrRect = dlg.GetLayerRect();
      CString strLayerName = dlg.GetLayerName();

      if (unFcls != SmtLayer_Ras)
        pDS.CreateVectorLayer(strLayerName, lyrRect, SmtFeatureType(unFcls));
      else
        pLayer = pDS.CreateRasterLayer(strLayerName, lyrRect, -1);

      SMT_SAFE_DELETE(pLayer);
    }

    UpdateCatalogTree();

    pDS.Close();
  }

  ReleaseCapture();
}

void SmtDSXCatalog::OnDsLayerDelete() {
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  SmtDataSource pDS = pDSMgr->get_data_source(GetSelDSName());

  if (pDS && pDS.Open()) {
    CString strLayerName = GetDSSelLayerName();
    CString strMessage;
    strMessage.Format("Delete layer %s?", strLayerName);
    if (IDOK == AfxMessageBox(strMessage, MB_OKCANCEL)) {
      if (pDS.DeleteVectorLayer(strLayerName)) {
        strMessage.Format("Delete layer %s?", strLayerName);
        AfxMessageBox(strMessage, MB_OK);
        UpdateCatalogTree();
      } else {
        strMessage.Format("Delete layer %s?", strLayerName);
        AfxMessageBox(strMessage, MB_OK);
      }
    }

    pDS.Close();
  }
}

void SmtDSXCatalog::OnDsLayerProperty() {
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();
  SmtDataSource pDS = pDSMgr->get_data_source(GetSelDSName());

  SmtVectorLayer *pSmtLayer = NULL;

  if (pDS && pDS.Open()) {
    CString strLayerName = GetDSSelLayerName();
    CString strMessage;
    if (pDS.GetLayerCount() > 0) {
      SmtLayerInfo info;
      pDS.GetLayerInfo(info, strLayerName);

      CString strMessage;
      strMessage.Format("Delete layer %s?", strLayerName);
      strMessage.TrimRight();
      AfxMessageBox(strMessage, MB_OK);
    }

    pDS.Close();
  }
}

void SmtDSXCatalog::OnDsProperty() {
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();
  SmtDataSource pDS = pDSMgr->get_data_source(GetSelDSName());

  SmtVectorLayer *pSmtLayer = NULL;

  if (pDS && pDS.Open()) {
    CString strMessage;
    if (pDS.GetLayerCount() > 0) {
      CString strMessage;
      strMessage.Format("Data Source Name:%s\nLayer Count:%d\nURL:%s",
                        pDS.GetName(), pDS.GetLayerCount(), pDS.GetUrl());
      strMessage.TrimRight();
      AfxMessageBox(strMessage, MB_OK);
    }

    pDS.Close();
  }
}

void SmtDSXCatalog::OnDsLayerLoadShp() {
  // TODO: 脭脷麓脣脤铆录脫脙眉脕卯麓娄脌铆鲁脤脨貌麓煤脗毛
  static char BASED_CODE szFilter[] =
      "Data Files (*.shp)|*.shp|All Files (*.*)|*.*||";

  CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  szFilter, NULL);

  if (dlg.DoModal() == IDCANCEL) {
    return;
  }

  if (dlg.GetPathName().IsEmpty()) {
    return;
  }
  //////////////////////////////////////////////////////////////////////////
  char szPath[_MAX_PATH];
  char szFileName[_MAX_PATH];
  char szTitle[_MAX_PATH];
  char szExt[_MAX_PATH];

  split_file_name(dlg.GetPathName(), szPath, szFileName, szTitle, szExt);
  //////////////////////////////////////////////////////////////////////////
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  SmtDataSource pTargetDS = pDSMgr->get_data_source(GetSelDSName());
  SmtDataSource pFileDS = pDSMgr->create_tmp_data_source(eDSType::DS_FILE_SMF);

  SmtVectorLayer *pTargetLayer = NULL;
  SmtVectorLayer *pFileLayer = NULL;

  CString strLayerName = GetDSSelLayerName();

  if (pTargetDS && pFileDS && pFileDS.Open() && pTargetDS.Open()) {
    pFileLayer = pFileDS.OpenVectorLayer(szTitle);
    pTargetLayer = pTargetDS.OpenVectorLayer(strLayerName);

    copy_layer(pTargetLayer, pFileLayer);

    pFileLayer->ResetReading();

    pFileDS.Close();
    pTargetDS.Close();

    /* OGR layer owned by dataset */
    /* OGR layer owned by dataset */

    pDSMgr->destroy_tmp_data_source(pFileDS);

    AfxMessageBox("瀵煎叆鎴愬姛!");
  }
}

void SmtDSXCatalog::OnDsLayerLoadImage() {
  static char BASED_CODE szFilter[] =
      "bmp Files (*.bmp)|*.bmp|\
											gif Files (*.gif)|*.gif|\
											jpg Files (*.jpg)|*.jpg|\
											ico Files (*.ico)|*.png|\
											tif Files (*.tif)|*.tif|\
											tga Files (*.tga)|*.tga|\
											pcx Files (*.pcx)|*.bmp|\
											wbmp Files (*.wbmp)|*.wbmp|\
											wmf Files (*.wmf)|*.wmf|\
											jpc Files (*.jpc)|*.jpc|\
											jp2 Files (*.jp2)|*.jp2|\
											pgx Files (*.pgx)|*.pgx|\
											pnm Files (*.pnm)|*.pnm|\
											ras Files (*.ras)|*.ras|";

  CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                  szFilter, NULL);

  if (dlg.DoModal() == IDCANCEL) {
    return;
  }

  if (dlg.GetPathName().IsEmpty()) {
    return;
  }
  //////////////////////////////////////////////////////////////////////////
  char szPath[_MAX_PATH];
  char szFileName[_MAX_PATH];
  char szTitle[_MAX_PATH];
  char szExt[_MAX_PATH];

  split_file_name(dlg.GetPathName(), szPath, szFileName, szTitle, szExt);
  //////////////////////////////////////////////////////////////////////////
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  SmtDataSource pTargetDS = pDSMgr->get_data_source(GetSelDSName());
  SmtDataSource pFileDS = pDSMgr->create_tmp_data_source(eDSType::DS_FILE_SMF);

  SmtRasterLayer *pTargetLayer = NULL;
  SmtRasterLayer *pFileLayer = NULL;

  CString strLayerName = GetDSSelLayerName();

  if (pTargetDS && pFileDS && pFileDS.Open() && pTargetDS.Open()) {
    pFileLayer = pFileDS.OpenRasterLayer(szFileName);
    pTargetLayer = pTargetDS.OpenRasterLayer(strLayerName);

    copy_layer(pTargetLayer, pFileLayer);

    pFileDS.Close();
    pTargetDS.Close();

    if (pFileLayer) {
      /* OGR layer owned by dataset */
    }

    if (pTargetLayer) {
      /* OGR layer owned by dataset */
    }

    pDSMgr->destroy_tmp_data_source(pFileDS);

    AfxMessageBox("瀵煎叆鎴愬姛!");
  }
}

void SmtDSXCatalog::OnDsDelete() {
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();
  CString strSelDSName = GetSelDSName();
  CString strMessage;
  strMessage.Format("Delete datasource %s?", strSelDSName);
  if (IDOK == AfxMessageBox(strMessage, MB_OKCANCEL)) {
    pDSMgr->delete_data_source(strSelDSName);

    UpdateCatalogTree();
  }
}

void SmtDSXCatalog::OnSvrDsCreate() {
  // TODO: 鍦ㄦ娣诲姞鍛戒护澶勭悊绋嬪簭浠ｇ爜
}

void SmtDSXCatalog::OnSvrDsAppend() {
  // TODO: 鍦ㄦ娣诲姞鍛戒护澶勭悊绋嬪簭浠ｇ爜
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  ::SetCapture(AfxGetMainWnd()->m_hWnd);

  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  CDlgCreateDS dlg(this);
  if (dlg.DoModal() == IDOK) {
    SmtDataSource pDS = pDSMgr->create_data_source(dlg.m_dsInfo);
    if (pDS) {
      UpdateCatalogTree();
    }
  }

  ReleaseCapture();
}

void SmtDSXCatalog::OnSvrDsSetActive() {
  // TODO: 鍦ㄦ娣诲姞鍛戒护澶勭悊绋嬪簭浠ｇ爜
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  pDSMgr->set_active_data_source(GetSelDSName());
}
}  // namespace ui
