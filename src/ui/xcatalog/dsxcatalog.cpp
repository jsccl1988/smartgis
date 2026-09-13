// SmtDSXCatalog.cpp : 实现文件
//

#include "stdafx.h"
#include "ui/xcatalog/xcatalog_core.h"
#include "dsxcatalog.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

#include "ui/xcatalog/mapmgr.h"
#include "sdb/map/map.h"
#include "base/core/api.h"
#include "sys/sysmanager.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sdb/feature/feature_api.h"
#include "base/core/logmanager.h"

// using namespace Smt_GIS;
// using namespace Smt_SDEDevMgr;
// using namespace Smt_Sys;

#include "ui/xcatalog/dlg_create_layer.h"
#include "ui/xcatalog/dlg_create_ds.h"

// SmtDSXCatalog

namespace ui
{
	IMPLEMENT_DYNAMIC(SmtDSXCatalog, SmtXCatalog)

	SmtDSXCatalog::SmtDSXCatalog()
	{
		m_hContexMenu = NULL;
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		SmtDataSource pDS = pDSMgr->GetActiveDataSource();
		if (pDS)
		{
			m_strSelDSName = pDS.GetName();
		} 
	}

	SmtDSXCatalog::~SmtDSXCatalog()
	{

	}


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

	// SmtDSXCatalog 消息处理程序
	bool SmtDSXCatalog::InitCreate(void) 
	{ 
		//AFX_MANAGE_STATE(AfxGetStaticModuleState());

#ifdef _DEBUG
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXCatalogCoreD.dll");
#else
		HINSTANCE   hInstance  =  ::GetModuleHandle("SmtXCatalogCore.dll");
#endif

		m_imgList.Create(16,16,ILC_COLOR16|ILC_MASK,1,0);

		m_imgList.SetBkColor(RGB(255,255,255));

		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_HOME)));
		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_DS)));
		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_LAYER)));
		m_imgList.Add(::LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON_DOC)));

		SetImageList(&m_imgList,TVSIL_NORMAL);

		return SmtXCatalog::InitCreate();
	}

	bool SmtDSXCatalog::EndDestory(void) 
	{ 
		return SmtXCatalog::EndDestory();
	}

	bool SmtDSXCatalog::CreateContexMenu()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		CMenu menuDSMgr;
		menuDSMgr.LoadMenu(IDR_MENU_DSMGR);
		m_hContexMenu = menuDSMgr.GetSafeHmenu();

		return SmtXCatalog::CreateContexMenu();
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL SmtDSXCatalog::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		// TODO: 在此添加专用代码和/或调用基类

		return SmtXCatalog::Create(dwStyle, rect, pParentWnd, nID);
	}

	int SmtDSXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (SmtXCatalog::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码

		return 0;
	}

	void SmtDSXCatalog::OnRButtonDown(UINT nFlags, CPoint point)
	{
		// TODO: 在此添加消息处理程序代码和/或调用默认值

		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		HTREEITEM hItem = HitTest(point,   &nFlags); 
		if((hItem != NULL)&&(TVHT_ONITEM&nFlags)) 
		{ 
			SelectItem(hItem); 
		}
		else 
			return;

		CMenu menuMapMgr;
		menuMapMgr.LoadMenu(IDR_MENU_DSMGR);

		CMenu* pMenu = NULL;
		HTREEITEM hParentItem= GetParentItem(hItem);

		if (hItem == m_hDSCatalog)
		{
			pMenu = menuMapMgr.GetSubMenu(2);
		}
		else if (hParentItem == m_hDSCatalog)
		{
			pMenu = menuMapMgr.GetSubMenu(1);
			m_strSelDSName= GetItemText(hItem);	
		}
		else
		{
			pMenu = menuMapMgr.GetSubMenu(0);
			m_strSelDSName= GetItemText(hParentItem);	
			m_strSelDSLayerName= GetItemText(hItem);	
		}

		if (pMenu)
		{
			CPoint menuPos;
			GetCursorPos(&menuPos); 
			pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,menuPos.x,menuPos.y,this);
			menuMapMgr.Detach();
		}

		SmtXCatalog::OnRButtonDown(nFlags, point);
	}

	void SmtDSXCatalog::UpdateCatalogTree(void)
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		SetRedraw(FALSE);
		DeleteAllItems();
		SetTextColor(RGB(0,0,255));
		m_hDSCatalog =InsertItem("数据源目录",0,0,TVI_ROOT);

		pDSMgr->MoveFirst();
		while (!pDSMgr->IsEnd())
		{
			AppendDSNode(pDSMgr->GetDataSource());
			pDSMgr->MoveNext();
		}

		Expand(m_hDSCatalog,TVE_EXPAND);

		SetRedraw(TRUE);

		RedrawWindow();		
	}

	void SmtDSXCatalog::AppendDSNode(SmtDataSource pDS)
	{
		if (pDS && pDS.Open())
		{
			HTREEITEM hDS = InsertItem(pDS.GetName(),1,1,m_hDSCatalog);

			SmtLayerInfo layerArchiveInfo;
			int nLayers = pDS.GetLayerCount();
			for (int i = 0; i < nLayers ; i++)
			{
				pDS.GetLayerInfo(layerArchiveInfo,i);
				HTREEITEM hLayerNode = InsertItem(layerArchiveInfo.szName,2,2,hDS);
				InsertItem(layerArchiveInfo.szArchiveName,3,3,hLayerNode);
				InsertItem(SmtDataSource::GetLayerFeatureTypeName(layerArchiveInfo.unFeatureType),3,3,hLayerNode);
				//Expand(hLayerNode,TVE_EXPAND);
			}	

			pDS.Close();
		}	
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtDSXCatalog::OnDsLayerCreate()
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		SmtDataSource pDS = pDSMgr->GetDataSource(GetSelDSName());

		if (pDS && pDS.Open())
		{
			SmtLayer * pLayer = NULL;
			CDlgCreateLayer dlg(this);
			if (dlg.DoModal() == IDOK)
			{
				UINT unFcls = dlg.GetSelFcls();
				fRect lyrRect = dlg.GetLayerRect();
				CString strLayerName = dlg.GetLayerName();

				if (unFcls != SmtLayer_Ras)
					pDS.CreateVectorLayer(strLayerName,lyrRect,SmtFeatureType(unFcls));
				else
					pLayer = pDS.CreateRasterLayer(strLayerName,lyrRect,-1);

				SMT_SAFE_DELETE(pLayer);
			}

			UpdateCatalogTree();

			pDS.Close();
		}

		ReleaseCapture();
	}

	void SmtDSXCatalog::OnDsLayerDelete()
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		SmtDataSource pDS = pDSMgr->GetDataSource(GetSelDSName());

		if (pDS && pDS.Open())
		{
			CString strLayerName = GetDSSelLayerName();
			CString strMessage;
			strMessage.Format("确定删除图层 %s？",strLayerName);
			if (IDOK == AfxMessageBox(strMessage,MB_OKCANCEL) )
			{
				if(pDS.DeleteVectorLayer(strLayerName))
				{
					strMessage.Format("图层 %s删除成功!",strLayerName);
					AfxMessageBox(strMessage,MB_OK);
					UpdateCatalogTree();
				}
				else
				{
					strMessage.Format("图层 %s删除失败!",strLayerName);
					AfxMessageBox(strMessage,MB_OK);
				}	
			}

			pDS.Close();
		}	
	}

	void SmtDSXCatalog::OnDsLayerProperty()
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		SmtDataSource pDS = pDSMgr->GetDataSource(GetSelDSName());

		SmtVectorLayer *pSmtLayer = NULL;

		if (pDS && pDS.Open())
		{
			CString strLayerName = GetDSSelLayerName();
			CString strMessage;
			if (pDS.GetLayerCount() > 0)
			{
				SmtLayerInfo info;
				pDS.GetLayerInfo(info,strLayerName);

				CString strMessage;
				strMessage.Format("LayerName: %s \nLayerAchive:%s \nLayerType:%s",strLayerName,info.szArchiveName,SmtDataSource::GetLayerFeatureTypeName(info.unFeatureType));
				strMessage.TrimRight();
				AfxMessageBox(strMessage,MB_OK);
			}	

			pDS.Close();
		}	
	}

	void SmtDSXCatalog::OnDsProperty()
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		SmtDataSource pDS = pDSMgr->GetDataSource(GetSelDSName());

		SmtVectorLayer *pSmtLayer = NULL;

		if (pDS && pDS.Open())
		{
			CString strMessage;
			if (pDS.GetLayerCount() > 0)
			{
				CString strMessage;
				strMessage.Format("Data Source Name:%s\nLayer Count:%d\nURL:%s",pDS.GetName(),pDS.GetLayerCount(),pDS.GetUrl());
				strMessage.TrimRight();
				AfxMessageBox(strMessage,MB_OK);
			}	

			pDS.Close();
		}	
	}

	void SmtDSXCatalog::OnDsLayerLoadShp()
	{
		// TODO: ÔÚ´ËÌí¼ÓÃüÁî´¦Àí³ÌÐò´úÂë
		static char BASED_CODE szFilter[] = "Data Files (*.shp)|*.shp|All Files (*.*)|*.*||" ;

		CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

		if( dlg.DoModal() == IDCANCEL )
		{
			return;
		}

		if (dlg.GetPathName().IsEmpty())
		{
			return;
		}
		//////////////////////////////////////////////////////////////////////////
		char szPath[_MAX_PATH];
		char szFileName[_MAX_PATH];
		char szTitle[_MAX_PATH];
		char szExt[_MAX_PATH];

		split_file_name(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);
		//////////////////////////////////////////////////////////////////////////
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		SmtDataSource pTargetDS = pDSMgr->GetDataSource(GetSelDSName());
		SmtDataSource pFileDS = pDSMgr->CreateTmpDataSource(eDSType::DS_FILE_SMF);

		SmtVectorLayer *pTargetLayer = NULL;
		SmtVectorLayer *pFileLayer = NULL;

		CString strLayerName = GetDSSelLayerName();

		if (pTargetDS && pFileDS&& pFileDS.Open() && pTargetDS.Open())
		{
			pFileLayer = pFileDS.OpenVectorLayer(szTitle);
			pTargetLayer = pTargetDS.OpenVectorLayer(strLayerName);

			copy_layer(pTargetLayer,pFileLayer);

			pFileLayer->ResetReading();

			pFileDS.Close();
			pTargetDS.Close();

			/* OGR layer owned by dataset */
			/* OGR layer owned by dataset */

			pDSMgr->DestoryTmpDataSource(pFileDS);

			AfxMessageBox( "导入成功!" ) ;
		}	
	}

	void SmtDSXCatalog::OnDsLayerLoadImage()
	{
		static char BASED_CODE szFilter[] = "bmp Files (*.bmp)|*.bmp|\
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

		CFileDialog dlg( TRUE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

		if( dlg.DoModal() == IDCANCEL )
		{
			return;
		}

		if (dlg.GetPathName().IsEmpty())
		{
			return;
		}
		//////////////////////////////////////////////////////////////////////////
		char szPath[_MAX_PATH];
		char szFileName[_MAX_PATH];
		char szTitle[_MAX_PATH];
		char szExt[_MAX_PATH];

		split_file_name(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);
		//////////////////////////////////////////////////////////////////////////
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		SmtDataSource pTargetDS = pDSMgr->GetDataSource(GetSelDSName());
		SmtDataSource pFileDS = pDSMgr->CreateTmpDataSource(eDSType::DS_FILE_SMF);

		SmtRasterLayer *pTargetLayer = NULL;
		SmtRasterLayer *pFileLayer = NULL;

		CString strLayerName = GetDSSelLayerName();

		if (pTargetDS && pFileDS&& pFileDS.Open() && pTargetDS.Open())
		{
			pFileLayer = pFileDS.OpenRasterLayer(szFileName);
			pTargetLayer = pTargetDS.OpenRasterLayer(strLayerName);

			copy_layer(pTargetLayer,pFileLayer);

			pFileDS.Close();
			pTargetDS.Close();

			if (pFileLayer)
			{
				/* OGR layer owned by dataset */
			}			

			if (pTargetLayer)
			{
				/* OGR layer owned by dataset */
			}	

			pDSMgr->DestoryTmpDataSource(pFileDS);

			AfxMessageBox( "导入成功!" ) ;
		}	
	}


	void SmtDSXCatalog::OnDsDelete()
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		CString strSelDSName = GetSelDSName();
		CString strMessage;
		strMessage.Format("确定删除图层 %s？",strSelDSName);
		if (IDOK == AfxMessageBox(strMessage,MB_OKCANCEL) )
		{
			pDSMgr->DeleteDataSource(strSelDSName);

			UpdateCatalogTree();
		}	
	}

	void SmtDSXCatalog::OnSvrDsCreate()
	{
		// TODO: 在此添加命令处理程序代码
	}

	void SmtDSXCatalog::OnSvrDsAppend()
	{
		// TODO: 在此添加命令处理程序代码
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		::SetCapture(AfxGetMainWnd()->m_hWnd);

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		CDlgCreateDS dlg(this);
		if (dlg.DoModal() == IDOK)
		{
			SmtDataSource pDS = pDSMgr->CreateDataSource(dlg.m_dsInfo);
			if (pDS)
			{
				UpdateCatalogTree();
			}		
		}

		ReleaseCapture();

	}

	void SmtDSXCatalog::OnSvrDsSetActive()
	{
		// TODO: 在此添加命令处理程序代码
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

		pDSMgr->SetActiveDataSource(GetSelDSName());
	}
}