// SmtMapDocXCatalog.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "ui/xcatalog/xcatalog_core.h"
#include "ui/xcatalog/mapdocxcatalog.h"

#include "ui/xcatalog/mapmgr.h"
#include "sdb/map/map.h"
#include "sdb/layer/layer.h"
#include "base/core/api.h"
#include "sys/sysmanager.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sdb/feature/feature_api.h"
#include "base/core/logmanager.h"
#include "legacy_tool/t_iatoolmanager.h"
#include "base/core/msg.h"
#include "legacy_tool/group/defs.h"
#include "ui/gui/gui_api.h"
#include "plugin/legacy/plugin_msg.h"
#include "legacy_tool/t_msg.h"

using namespace sdb;
using namespace sdb;
using namespace sys;


#include "ui/xcatalog/dlg_sel_layer.h"
#include "ui/xcatalog/dlg_create_map.h"
#include "ui/xcatalog/dlg_create_layer.h"
// SmtMapDocXCatalog

namespace ui
{
	IMPLEMENT_DYNAMIC(SmtMapDocXCatalog, SmtXCatalog)

		SmtMapDocXCatalog::SmtMapDocXCatalog()
	{
		m_hContexMenu = NULL;
	}

	SmtMapDocXCatalog::~SmtMapDocXCatalog()
	{

	}


	BEGIN_MESSAGE_MAP(SmtMapDocXCatalog, CTreeCtrl)
		ON_WM_RBUTTONDOWN()
		ON_WM_LBUTTONDOWN()
		ON_WM_CREATE()

		ON_COMMAND(ID_LAYER_MGR_APPEND, &SmtMapDocXCatalog::OnLayerMgrAppend)
		ON_COMMAND(ID_LAYER_MGR_REMOVE, &SmtMapDocXCatalog::OnLayerMgrRemove)
		ON_COMMAND(ID_LAYER_MGR_ACTIVE, &SmtMapDocXCatalog::OnLayerMgrActive)
		ON_COMMAND(ID_LAYER_MGR_PROPERTY, &SmtMapDocXCatalog::OnLayerMgrProperty)
		ON_COMMAND(ID_LAYER_MGR_CALCMBR, &SmtMapDocXCatalog::OnLayerMgrReCalcMBR)
		ON_COMMAND(ID_LAYER_MGR_ATTSTRUCT, &SmtMapDocXCatalog::OnLayerMgrAttstruct)

		ON_COMMAND(ID_MAP_MGR_NEW, &SmtMapDocXCatalog::OnMapMgrNew)
		ON_COMMAND(ID_MAP_MGR_OPEN, &SmtMapDocXCatalog::OnMapMgrOpen)
		ON_COMMAND(ID_MAP_MGR_CLOSE, &SmtMapDocXCatalog::OnMapMgrClose)
		ON_COMMAND(ID_MAP_MGR_SAVE, &SmtMapDocXCatalog::OnMapMgrSave)
		ON_COMMAND(ID_MAP_MGR_SAVEAS, &SmtMapDocXCatalog::OnMapMgrSaveas)

	END_MESSAGE_MAP()

	// SmtMapDocXCatalog ��Ϣ��������
	bool SmtMapDocXCatalog::InitCreate(void) 
	{ 
		return SmtXCatalog::InitCreate();
	}

	bool SmtMapDocXCatalog::EndDestory(void) 
	{ 
		return SmtXCatalog::EndDestory();
	}

	bool SmtMapDocXCatalog::CreateContexMenu()
	{
		m_hContexMenu = ::CreatePopupMenu();
		return SmtXCatalog::CreateContexMenu();
	}

	int SmtMapDocXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (SmtXCatalog::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  �ڴ�������ר�õĴ�������
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		m_imgList.Create(IDB_LAYER_STATE,13,1,RGB(255,255,255));
		SetImageList(&m_imgList,TVSIL_STATE);

		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		pMapMgr->RegisterMapCatalog((void*)this);

		return 0;
	}


	void SmtMapDocXCatalog::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,&m_nFlags); 
		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
			SelectItem(hItem);
		else 
			return;

		CMenu menuMapMgr;
		menuMapMgr.LoadMenu(IDR_MENU_MAPMGR);

		CMenu* pMenu = NULL;
		HTREEITEM hParentItem=GetParentItem(hItem);
		if (hItem == m_hRoot)
		{
			pMenu = menuMapMgr.GetSubMenu(1);
		}
		else if (hParentItem == m_hMap || hItem == m_hMap)
		{
			m_strSelLayerName = GetItemText(hItem);
			pMenu = menuMapMgr.GetSubMenu(0);
		}

		if (pMenu)
		{
			CPoint      menuPos;   
			GetCursorPos(&menuPos);  

			pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,menuPos.x,menuPos.y,this);
			pMenu->Detach();
		}		

		SmtXCatalog::OnRButtonDown(nFlags, point);
	}

	void SmtMapDocXCatalog::OnLButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ

		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,&nFlags); 

		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		SmtMap *pSmtMap = SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr();
		if (NULL != pSmtMap)
		{
			if ((nFlags&TVHT_ONITEMSTATEICON ) && (hItem != m_hRoot) && (hItem !=m_hMap))
			{
				HTREEITEM hParentItem=GetParentItem(hItem);
				UINT nState = GetItemState( hItem, TVIS_STATEIMAGEMASK )>>12;

				if (hParentItem == m_hMap)
				{	
					nState =(nState == 3)? 1:3;
					SetItemState( hItem, INDEXTOSTATEIMAGEMASK(nState), TVIS_STATEIMAGEMASK );
					bool bIsVisible = (nState == 3);
					SmtLayer *pLayer = pSmtMap->GetLayer(GetItemText(hItem)); 
					if(pLayer)
					{
						pLayer->SetVisible(bIsVisible);
						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;
						post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
					}
				}
				else if (hParentItem == m_hRoot)
				{
					SetItemState( hItem, INDEXTOSTATEIMAGEMASK(nState), TVIS_STATEIMAGEMASK );
					SelectItem(hItem);
				}
			}
		}

		SmtXCatalog::OnLButtonDown(nFlags, point);
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtMapDocXCatalog::UpdateMapTree()
	{
		DeleteAllItems();
		SetRedraw(FALSE);
		SetTextColor(RGB(0,0,255));

		m_hRoot =InsertItem("��ͼ�ĵ�Ŀ¼");
		SetItemState(m_hRoot, INDEXTOSTATEIMAGEMASK(0), TVIS_STATEIMAGEMASK);

		SmtMap *pSmtMap = SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr();
		if (NULL == pSmtMap)
			return false;

		m_hMap = InsertItem(pSmtMap->GetMapName(),m_hRoot);
		SetItemState(m_hMap, INDEXTOSTATEIMAGEMASK(2), TVIS_STATEIMAGEMASK);
		CString layername;
		CString objectname;

		pSmtMap->MoveFirst();
		while (!pSmtMap->IsEnd())
		{
			SmtLayer *pLayer =  pSmtMap->GetLayer();
			HTREEITEM hLayer = InsertItem(pLayer->GetLayerName(),m_hMap);

			if (pLayer->IsVisible())
				SetItemState(hLayer, INDEXTOSTATEIMAGEMASK(3), TVIS_STATEIMAGEMASK);
			else 
				SetItemState(hLayer, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);

			pSmtMap->MoveNext();
		}

		Expand(m_hRoot,TVE_EXPAND);
		Expand(m_hMap,TVE_EXPAND);
		SetRedraw(TRUE);
		RedrawWindow();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtMapDocXCatalog::OnLayerMgrAppend()
	{
		// TODO: �ڴ�����������������
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		if (pMapMgr && pDSMgr)
		{
			CDlgSelLayer dlg(this);
			if (dlg.DoModal() == IDOK)
			{
				CString strDSName = dlg.GetSelDSName();
				SmtDataSource pDS = pDSMgr->GetDataSource((LPCTSTR)strDSName);

				if (pDS && pDS.Open() && pDS.GetLayerCount() > 0)
				{
					CString strLayerName = dlg.GetSelLayerName();
					SmtLayer *pLayer = pMapMgr->GetLayer(strLayerName);
					OGRLayer *pOgr = pMapMgr->GetSmtMapPtr()
						? pMapMgr->GetSmtMapPtr()->GetOgrLayer(strLayerName)
						: NULL;
					if (pLayer == NULL && pOgr == NULL)
					{
						SmtLayerInfo lyrInfo;
						pDS.GetLayerInfo(lyrInfo,strLayerName);

						if (lyrInfo.unFeatureType == SmtLayer_Ras)
						{
							pLayer = pDS.OpenRasterLayer(strLayerName);
							if (pLayer && pMapMgr->AppendLayer(pLayer))
							{	
								SmtListenerMsg param;
								param.hSrcWnd = m_hWnd;
								post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
							}
						}
						else
						{
							OGRLayer *vl = pDS.OpenVectorLayer(strLayerName);
							if (vl && pMapMgr->AppendLayer(vl))
							{
								SmtListenerMsg param;
								param.hSrcWnd = m_hWnd;
								post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
							}
						}
					}	
					else
					{
						CString strMessage;
						strMessage.Format("ͼ�� %s�Ѿ������� %s !",strLayerName,pMapMgr->GetSmtMapPtr()->GetMapName());
						AfxMessageBox(strMessage,MB_OK);
					}	
					pDS.Close();
				}	
			}
		}

		ReleaseCapture();
	}

	void SmtMapDocXCatalog::OnLayerMgrRemove()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			bool bRet = pMapMgr->DeleteLayer(GetMapSelLayerName());
			if (bRet)
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
				//UpdateMapTree();
			}	
		}
	}

	void SmtMapDocXCatalog::OnLayerMgrActive()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			pMapMgr->SetActiveLayer(GetMapSelLayerName());
		}
	}

	void SmtMapDocXCatalog::OnLayerMgrAttstruct()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			SmtMap *pMap = pMapMgr->GetSmtMapPtr();
			OGRLayer *pOgr = pMap ? pMap->GetOgrLayer(GetMapSelLayerName()) : NULL;
			if (pOgr)
			{
				SmtAttStructEditDlg(pOgr, 1);
			}
			else
			{
				CString strMessage;
				strMessage.Format(
					_T("Selected layer has no OGR schema (raster/tile leftover)."));
				AfxMessageBox(strMessage, MB_OK);
			}
		}

		ReleaseCapture();
	}

	void SmtMapDocXCatalog::OnLayerMgrReCalcMBR()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			SmtLayer * pLayer  = pMapMgr->GetLayer(GetMapSelLayerName());
			if (pLayer)
			{	
				pLayer->CalEnvelope();
			}
		}
	}

	void SmtMapDocXCatalog::OnLayerMgrProperty()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			SmtLayer * pLayer  = pMapMgr->GetLayer(GetMapSelLayerName());
			OGRLayer * pOgr = pMapMgr->GetSmtMapPtr()
				? pMapMgr->GetSmtMapPtr()->GetOgrLayer(GetMapSelLayerName())
				: NULL;
			if (pOgr)
			{
				CString strMessage;
				strMessage.Format("ͼ��:%s Feature Count %d", pOgr->GetName(),
					static_cast<int>(pOgr->GetFeatureCount()));
				AfxMessageBox(strMessage,MB_OK);
			}
			else if (pLayer)
			{	
				CString strMessage;
				strMessage.Format("ͼ��:%s ͼ������:%s",pLayer->GetLayerName(),
					pLayer->GetLayerType() == LYR_RASTER ? "Raster" : "Tile");
				AfxMessageBox(strMessage,MB_OK);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtMapDocXCatalog::OnMapMgrNew()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			CDlgCreateMap dlg(this);
			if (dlg.DoModal() == IDOK)
			{
				CString strMapName = dlg.GetMapName();
				pMapMgr->NewMap(strMapName);
				UpdateMapTree();
			}
		}

		ReleaseCapture();
	}

	void SmtMapDocXCatalog::OnMapMgrOpen()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||" ;

			CFileDialog dlg( TRUE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

			if( dlg.DoModal() == IDCANCEL ||
				dlg.GetPathName().IsEmpty())
			{
				return;
			}

			pMapMgr->OpenMap(dlg.GetPathName());
			UpdateMapTree();
		}
	}

	void SmtMapDocXCatalog::OnMapMgrClose()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			pMapMgr->CloseMap();
			UpdateMapTree();
		}
	}

	void SmtMapDocXCatalog::OnMapMgrSave()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			pMapMgr->SaveMap();
		}
	}

	void SmtMapDocXCatalog::OnMapMgrSaveas()
	{
		// TODO: �ڴ�����������������
		SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (pMapMgr)
		{
			static char BASED_CODE szFilter[] = "Data Files (*.mdoc)|*.mdoc||" ;

			CFileDialog dlg( FALSE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

			if( dlg.DoModal() == IDCANCEL ||
				dlg.GetPathName().IsEmpty())
			{
				return;
			}

			pMapMgr->SaveMapAs(dlg.GetPathName());
		}
	}
}