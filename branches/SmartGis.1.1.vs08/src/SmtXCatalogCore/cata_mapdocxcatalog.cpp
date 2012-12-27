// SmtMapDocXCatalog.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "cata_mapdocxcatalog.h"

#include "cata_mapmgr.h"
#include "gis_map.h"
#include "gis_sde.h"
#include "smt_api.h"
#include "sys_sysmanager.h"
#include "sde_datasourcemgr.h"
#include "gis_api.h"
#include "smt_logmanager.h"
#include "t_iatoolmanager.h"
#include "smt_msg.h"
#include "gt_defs.h"
#include "smt_gui_api.h"
#include "am_msg.h"
#include "t_msg.h"

using namespace Smt_SDEDevMgr;
using namespace Smt_GIS;
using namespace Smt_Sys;


#include "DlgSelLayer.h"
#include "DlgCreateMap.h"
#include "DlgCreateLayer.h"
// SmtMapDocXCatalog

namespace Smt_XCatalog
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

	// SmtMapDocXCatalog 消息处理程序
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

		// TODO:  在此添加您专用的创建代码
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		m_imgList.Create(IDB_LAYER_STATE,13,1,RGB(255,255,255));
		SetImageList(&m_imgList,TVSIL_STATE);

		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		pMapMgr->RegisterMapCatalog((void*)this);

		return 0;
	}


	void SmtMapDocXCatalog::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值
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
		// TODO: 在此添加消息处理程序代码和/或调用默认值

		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,&nFlags); 

		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		SmtMap *pSmtMap = SmtMapMgr::GetSingletonPtr()->GetSmtMapPtr();
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
						SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
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

		m_hRoot =InsertItem("地图文档目录");
		SetItemState(m_hRoot, INDEXTOSTATEIMAGEMASK(0), TVIS_STATEIMAGEMASK);

		SmtMap *pSmtMap = SmtMapMgr::GetSingletonPtr()->GetSmtMapPtr();
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
		// TODO: 在此添加命令处理程序代码
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr();

		if (pMapMgr && pDSMgr)
		{
			CDlgSelLayer dlg(this);
			if (dlg.DoModal() == IDOK)
			{
				CString strDSName = dlg.GetSelDSName();
				SmtDataSource *pDS = pDSMgr->GetDataSource((LPCTSTR)strDSName);

				if (NULL != pDS && pDS->Open() && pDS->GetLayerCount() > 0)
				{
					CString strLayerName = dlg.GetSelLayerName();
					SmtLayer *pLayer = NULL;
					pLayer = pMapMgr->GetLayer(strLayerName);
					if (pLayer == NULL)
					{
						SmtLayerInfo lyrInfo;
						pDS->GetLayerInfo(lyrInfo,strLayerName);

						if (lyrInfo.unFeatureType == SmtLayer_Ras)
							pLayer = pDS->OpenRasterLayer(strLayerName);
						else 
							pLayer = pDS->OpenVectorLayer(strLayerName);	

						if (pLayer && pMapMgr->AppendLayer(pLayer))
						{	
							SmtListenerMsg param;
							param.hSrcWnd = m_hWnd;
							SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
							//UpdateMapTree();
						}	
					}	
					else
					{
						CString strMessage;
						strMessage.Format("图层 %s已经添加至 %s !",strLayerName,pMapMgr->GetSmtMapPtr()->GetMapName());
						AfxMessageBox(strMessage,MB_OK);
					}	
					pDS->Close();
				}	
			}
		}

		ReleaseCapture();
	}

	void SmtMapDocXCatalog::OnLayerMgrRemove()
	{
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			bool bRet = pMapMgr->DeleteLayer(GetMapSelLayerName());
			if (bRet)
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,m_hWnd),param);
				//UpdateMapTree();
			}	
		}
	}

	void SmtMapDocXCatalog::OnLayerMgrActive()
	{
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			pMapMgr->SetActiveLayer(GetMapSelLayerName());
		}
	}

	void SmtMapDocXCatalog::OnLayerMgrAttstruct()
	{
		// TODO: 在此添加命令处理程序代码
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			SmtLayer * pLayer  = pMapMgr->GetLayer(GetMapSelLayerName());
			if (pLayer)
			{	
				SmtAttribute *pAtt = pLayer->GetAttribute();
				SmtAttStructEditDlg(pAtt,1);
			}	
			else
			{
				CString strMessage;
				strMessage.Format("获取图层失败！");
				AfxMessageBox(strMessage,MB_OK);
			}
		}

		ReleaseCapture();
	}

	void SmtMapDocXCatalog::OnLayerMgrReCalcMBR()
	{
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
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
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			SmtLayer * pLayer  = pMapMgr->GetLayer(GetMapSelLayerName());
			if (pLayer)
			{	
				if (LYR_VECTOR == pLayer->GetLayerType())
				{
					SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
					CString strMessage;
					strMessage.Format("图层:%s 图层类型:%s Feature Count %d",pVLayer->GetLayerName(),SmtDataSource::GetLayerFeatureTypeName(pVLayer->GetLayerFeatureType()),pVLayer->GetFeatureCount());
					AfxMessageBox(strMessage,MB_OK);
				}
				else if(LYR_RASTER == pLayer->GetLayerType())
				{
					SmtRasterLayer *pRLayer = (SmtRasterLayer *)pLayer;
					CString strMessage;
					strMessage.Format("图层:%s 图层类型:%s",pRLayer->GetLayerName(),toString(LYR_RASTER));
					AfxMessageBox(strMessage,MB_OK);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtMapDocXCatalog::OnMapMgrNew()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
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
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
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
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			pMapMgr->CloseMap();
			UpdateMapTree();
		}
	}

	void SmtMapDocXCatalog::OnMapMgrSave()
	{
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (pMapMgr)
		{
			pMapMgr->SaveMap();
		}
	}

	void SmtMapDocXCatalog::OnMapMgrSaveas()
	{
		// TODO: 在此添加命令处理程序代码
		SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
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