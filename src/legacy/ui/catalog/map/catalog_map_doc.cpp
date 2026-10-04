// MapDocXCatalog.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "legacy/ui/catalog/map/catalog_map_doc.h"

#include "base/core/log.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatoolmanager.h"
#include "legacy/tool/abi/t_msg.h"
#include "legacy/ui/dialogs/dialogs_api.h"
#include "legacy/ui/catalog/map/mapmgr.h"
#include "legacy/ui/catalog/resource.h"
#include "legacy/plugin/runtime/auxmodule/plugin_msg.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "legacy/gis/feature/leftover_copy_layer.h"
#include "legacy/gis/layer/layer.h"
#include "legacy/gis/layer/map_bind.h"
#include "gis/map/map.h"
#include "legacy/sys/sysmanager.h"

using namespace gis;
using namespace gis;
using namespace sys;

#include "legacy/ui/catalog/map/dlg_create_layer.h"
#include "legacy/ui/catalog/map/dlg_create_map.h"
#include "legacy/ui/catalog/map/dlg_sel_layer.h"
// MapDocXCatalog

namespace ui {
IMPLEMENT_DYNAMIC(MapDocXCatalog, SmtXCatalog)

MapDocXCatalog::MapDocXCatalog() { m_hContexMenu = NULL; }

MapDocXCatalog::~MapDocXCatalog() {}

BEGIN_MESSAGE_MAP(MapDocXCatalog, CTreeCtrl)
ON_WM_RBUTTONDOWN()
ON_WM_LBUTTONDOWN()
ON_WM_CREATE()

ON_COMMAND(ID_LAYER_MGR_APPEND, &MapDocXCatalog::OnLayerMgrAppend)
ON_COMMAND(ID_LAYER_MGR_REMOVE, &MapDocXCatalog::OnLayerMgrRemove)
ON_COMMAND(ID_LAYER_MGR_ACTIVE, &MapDocXCatalog::OnLayerMgrActive)
ON_COMMAND(ID_LAYER_MGR_PROPERTY, &MapDocXCatalog::OnLayerMgrProperty)
ON_COMMAND(ID_LAYER_MGR_CALCMBR, &MapDocXCatalog::OnLayerMgrReCalcMBR)
ON_COMMAND(ID_LAYER_MGR_ATTSTRUCT, &MapDocXCatalog::OnLayerMgrAttstruct)

ON_COMMAND(ID_MAP_MGR_NEW, &MapDocXCatalog::OnMapMgrNew)
ON_COMMAND(ID_MAP_MGR_OPEN, &MapDocXCatalog::OnMapMgrOpen)
ON_COMMAND(ID_MAP_MGR_CLOSE, &MapDocXCatalog::OnMapMgrClose)
ON_COMMAND(ID_MAP_MGR_SAVE, &MapDocXCatalog::OnMapMgrSave)
ON_COMMAND(ID_MAP_MGR_SAVEAS, &MapDocXCatalog::OnMapMgrSaveas)

END_MESSAGE_MAP()

// MapDocXCatalog ��Ϣ��������
bool MapDocXCatalog::InitCreate(void) { return SmtXCatalog::InitCreate(); }

bool MapDocXCatalog::EndDestory(void) { return SmtXCatalog::EndDestory(); }

bool MapDocXCatalog::CreateContexMenu() {
  m_hContexMenu = ::CreatePopupMenu();
  return SmtXCatalog::CreateContexMenu();
}

int MapDocXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (SmtXCatalog::OnCreate(lpCreateStruct) == -1) return -1;

  // TODO:  �ڴ�������ר�õĴ�������
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  m_imgList.Create(IDB_LAYER_STATE, 13, 1, RGB(255, 255, 255));
  SetImageList(&m_imgList, TVSIL_STATE);

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  pMapMgr->RegisterMapCatalog((void *)this);

  return 0;
}

void MapDocXCatalog::OnRButtonDown(UINT nFlags, CPoint point) {
  // TODO: restored after encoding merge
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  HTREEITEM hItem = HitTest(point, &m_nFlags);
  if ((hItem != NULL) && (TVHT_ONITEM & nFlags))
    SelectItem(hItem);
  else
    return;

  CMenu menuMapMgr;
  menuMapMgr.LoadMenu(IDR_MENU_MAPMGR);

  CMenu *pMenu = NULL;
  HTREEITEM hParentItem = GetParentItem(hItem);
  if (hItem == m_hRoot) {
    pMenu = menuMapMgr.GetSubMenu(1);
  } else if (hParentItem == m_hMap || hItem == m_hMap) {
    m_strSelLayerName = GetItemText(hItem);
    pMenu = menuMapMgr.GetSubMenu(0);
  }

  if (pMenu) {
    CPoint menuPos;
    GetCursorPos(&menuPos);

    pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                          menuPos.x, menuPos.y, this);
    pMenu->Detach();
  }

  SmtXCatalog::OnRButtonDown(nFlags, point);
}

void MapDocXCatalog::OnLButtonDown(UINT nFlags, CPoint point) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ���?
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  HTREEITEM hItem = HitTest(point, &nFlags);

  if ((hItem != NULL) && (TVHT_ONITEM & nFlags)) {
    SelectItem(hItem);
  } else
    return;

  Map *pSmtMap = SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr();
  if (NULL != pSmtMap) {
    if ((nFlags & TVHT_ONITEMSTATEICON) && (hItem != m_hRoot) &&
        (hItem != m_hMap)) {
      HTREEITEM hParentItem = GetParentItem(hItem);
      UINT nState = GetItemState(hItem, TVIS_STATEIMAGEMASK) >> 12;

      if (hParentItem == m_hMap) {
        nState = (nState == 3) ? 1 : 3;
        SetItemState(hItem, INDEXTOSTATEIMAGEMASK(nState), TVIS_STATEIMAGEMASK);
        bool bIsVisible = (nState == 3);
        Layer *pLayer =
            leftover_layer_named(pSmtMap, CStringA(GetItemText(hItem)));
        if (pLayer) {
          pLayer->SetVisible(bIsVisible);
          SmtListenerMsg param;
          param.hSrcWnd = m_hWnd;
          post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                           SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, m_hWnd), param);
        }
      } else if (hParentItem == m_hRoot) {
        SetItemState(hItem, INDEXTOSTATEIMAGEMASK(nState), TVIS_STATEIMAGEMASK);
        SelectItem(hItem);
      }
    }
  }

  SmtXCatalog::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
bool MapDocXCatalog::UpdateMapTree() {
  DeleteAllItems();
  SetRedraw(FALSE);
  SetTextColor(RGB(0, 0, 255));

  m_hRoot = InsertItem("Map catalog");
  SetItemState(m_hRoot, INDEXTOSTATEIMAGEMASK(0), TVIS_STATEIMAGEMASK);

  Map *pSmtMap = SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr();
  if (NULL == pSmtMap) return false;

  m_hMap = InsertItem(pSmtMap->GetMapName(), m_hRoot);
  SetItemState(m_hMap, INDEXTOSTATEIMAGEMASK(2), TVIS_STATEIMAGEMASK);

  // Walk by index: OGR layers have no leftover Layer* (GetLayer() is
  // null). Use GetLayerName / IsLayerVisible so AppendLayer(OGR) is safe.
  const int n = pSmtMap->GetLayerCount();
  for (int i = 0; i < n; ++i) {
    const char *name = pSmtMap->GetLayerName(i);
    HTREEITEM hLayer = InsertItem(name && name[0] ? name : "(unnamed)", m_hMap);
    if (pSmtMap->IsLayerVisible(i))
      SetItemState(hLayer, INDEXTOSTATEIMAGEMASK(3), TVIS_STATEIMAGEMASK);
    else
      SetItemState(hLayer, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
  }

  Expand(m_hRoot, TVE_EXPAND);
  Expand(m_hMap, TVE_EXPAND);
  SetRedraw(TRUE);
  RedrawWindow();

  return true;
}

//////////////////////////////////////////////////////////////////////////
void MapDocXCatalog::OnLayerMgrAppend() {
  // TODO: restored after encoding merge
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  ::SetCapture(AfxGetMainWnd()->m_hWnd);

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  if (pMapMgr && pDSMgr) {
    CDlgSelLayer dlg(this);
    if (dlg.DoModal() == IDOK) {
      CString strDSName = dlg.GetSelDSName();
      CatalogSource pDS = pDSMgr->get_data_source((LPCTSTR)strDSName);

      if (pDS && pDS.Open() && pDS.GetLayerCount() > 0) {
        CString strLayerName = dlg.GetSelLayerName();
        Layer *pLayer = pMapMgr->GetLayer(strLayerName);
        OGRLayer *pOgr =
            pMapMgr->GetSmtMapPtr()
                ? pMapMgr->GetSmtMapPtr()->GetOgrLayer(strLayerName)
                : NULL;
        if (pLayer == NULL && pOgr == NULL) {
          LayerInfo lyrInfo;
          pDS.GetLayerInfo(lyrInfo, strLayerName);

          if (lyrInfo.unFeatureType == LayerRas) {
            pLayer = pDS.OpenRasterLayer(strLayerName);
            if (pLayer && pMapMgr->AppendLayer(pLayer)) {
              SmtListenerMsg param;
              param.hSrcWnd = m_hWnd;
              post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                               SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, m_hWnd),
                               param);
            }
          } else {
            OGRLayer *vl = pDS.OpenVectorLayer(strLayerName);
            if (vl && pMapMgr->AppendLayer(vl)) {
              SmtListenerMsg param;
              param.hSrcWnd = m_hWnd;
              post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                               SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, m_hWnd),
                               param);
            }
          }
        } else {
          CString strMessage;
          strMessage.Format("ͼ�� %s�Ѿ������� %s !", strLayerName,
                            pMapMgr->GetSmtMapPtr()->GetMapName());
          AfxMessageBox(strMessage, MB_OK);
        }
        pDS.Close();
      }
    }
  }

  ReleaseCapture();
}

void MapDocXCatalog::OnLayerMgrRemove() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    bool bRet = pMapMgr->DeleteLayer(GetMapSelLayerName());
    if (bRet) {
      SmtListenerMsg param;
      param.hSrcWnd = m_hWnd;
      post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                       SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, m_hWnd), param);
      // UpdateMapTree();
    }
  }
}

void MapDocXCatalog::OnLayerMgrActive() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    pMapMgr->SetActiveLayer(GetMapSelLayerName());
  }
}

void MapDocXCatalog::OnLayerMgrAttstruct() {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  ::SetCapture(AfxGetMainWnd()->m_hWnd);

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    Map *pMap = pMapMgr->GetSmtMapPtr();
    OGRLayer *pOgr = pMap ? pMap->GetOgrLayer(GetMapSelLayerName()) : NULL;
    if (pOgr) {
      SmtAttStructEditDlg(pOgr, 1);
    } else {
      CString strMessage;
      strMessage.Format(
          _T("Selected layer has no OGR schema (raster/tile leftover)."));
      AfxMessageBox(strMessage, MB_OK);
    }
  }

  ReleaseCapture();
}

void MapDocXCatalog::OnLayerMgrReCalcMBR() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    Layer *pLayer = pMapMgr->GetLayer(GetMapSelLayerName());
    if (pLayer) {
      pLayer->CalEnvelope();
    }
  }
}

void MapDocXCatalog::OnLayerMgrProperty() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    Layer *pLayer = pMapMgr->GetLayer(GetMapSelLayerName());
    OGRLayer *pOgr =
        pMapMgr->GetSmtMapPtr()
            ? pMapMgr->GetSmtMapPtr()->GetOgrLayer(GetMapSelLayerName())
            : NULL;
    if (pOgr) {
      CString strMessage;
      strMessage.Format("ͼ��:%s Feature Count %d", pOgr->GetName(),
                        static_cast<int>(pOgr->GetFeatureCount()));
      AfxMessageBox(strMessage, MB_OK);
    } else if (pLayer) {
      CString strMessage;
      strMessage.Format(
          "ͼ��:%s ͼ������:%s", pLayer->GetLayerName(),
          pLayer->GetLayerType() == LYR_RASTER ? "Raster" : "Tile");
      AfxMessageBox(strMessage, MB_OK);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
void MapDocXCatalog::OnMapMgrNew() {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  ::SetCapture(AfxGetMainWnd()->m_hWnd);

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    CDlgCreateMap dlg(this);
    if (dlg.DoModal() == IDOK) {
      CString strMapName = dlg.GetMapName();
      pMapMgr->NewMap(strMapName);
      UpdateMapTree();
    }
  }

  ReleaseCapture();
}

void MapDocXCatalog::OnMapMgrOpen() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||";

    CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    szFilter, NULL);

    if (dlg.DoModal() == IDCANCEL || dlg.GetPathName().IsEmpty()) {
      return;
    }

    pMapMgr->OpenMap(dlg.GetPathName());
    UpdateMapTree();
  }
}

void MapDocXCatalog::OnMapMgrClose() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    pMapMgr->CloseMap();
    UpdateMapTree();
  }
}

void MapDocXCatalog::OnMapMgrSave() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    pMapMgr->SaveMap();
  }
}

void MapDocXCatalog::OnMapMgrSaveas() {
  // TODO: restored after encoding merge
  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||";

    CFileDialog dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    szFilter, NULL);

    if (dlg.DoModal() == IDCANCEL || dlg.GetPathName().IsEmpty()) {
      return;
    }

    pMapMgr->SaveMapAs(dlg.GetPathName());
  }
}
}  // namespace ui