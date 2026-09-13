// dlg_sel_layer.cpp : 实现文件
//

#include "stdafx.h"
#include "ui/xcatalog/xcatalog_core.h"
#include "ui/xcatalog/dlg_sel_layer.h"
#include "ui/xcatalog/dlg_sel_ds.h"
#include "base/core/api.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

// using namespace Smt_SDEDevMgr;


// CDlgSelLayer 对话框

IMPLEMENT_DYNAMIC(CDlgSelLayer, CDialog)

CDlgSelLayer::CDlgSelLayer(CWnd* pParent /*=NULL*/)
: CDialog(CDlgSelLayer::IDD, pParent)
, m_strSelLayerName(_T(""))
, m_strSelLayerArchive(_T(""))
, m_strSelLayerType(_T(""))
, m_strSelDSName(_T(""))
{

}

CDlgSelLayer::~CDlgSelLayer()
{

}

void CDlgSelLayer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DS_TREE, m_DsTree);
	DDX_Text(pDX, IDC_EDIT_LAYER_NAME, m_strSelLayerName);
	DDX_Text(pDX, IDC_EDIT_LAYER_ARCHIVE, m_strSelLayerArchive);
	DDX_Text(pDX, IDC_EDIT_LAYER_TYPE, m_strSelLayerType);
	DDX_Text(pDX, IDC_EDIT_DS_NAME, m_strSelDSName);
}


BEGIN_MESSAGE_MAP(CDlgSelLayer, CDialog)
	ON_BN_CLICKED(IDC_BTN_SEL_DS, &CDlgSelLayer::OnBnClickedBtnSelDs)
	ON_BN_CLICKED(IDOK, &CDlgSelLayer::OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_DS_TREE, &CDlgSelLayer::OnNMClickDsTree)
	ON_NOTIFY(NM_DBLCLK, IDC_DS_TREE, &CDlgSelLayer::OnNMDblclkDsTree)
END_MESSAGE_MAP()


// CDlgSelLayer 消息处理程序
BOOL CDlgSelLayer::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  ÔÚ´ËÌí¼Ó¶îÍâµÄ³õÊ¼»¯
	SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
	
	if (pDSMgr->GetDataSourceCount() > 0)
	{
		SmtDataSource pDS;

		pDS = pDSMgr->GetActiveDataSource();
		if (NULL == pDS )
		{
			pDSMgr->MoveFirst();
			pDS = pDSMgr->GetDataSource();
		}

		m_strSelDSName = pDS.GetName();

		UpdateDsTree();	
	}

	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	// Òì³£: OCX ÊôÐÔÒ³Ó¦·µ»Ø FALSE
}

void CDlgSelLayer::OnBnClickedBtnSelDs()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	CDlgSelDS dlg;
	if (dlg.DoModal() == IDOK && m_strSelDSName != dlg.GetSelDSName())
	{
		m_strSelDSName = dlg.GetSelDSName();
		UpdateDsTree();
		UpdateData(FALSE);
	}
}

void CDlgSelLayer::OnBnClickedOk()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	//ShowWindow(SW_HIDE);
	OnOK();
}

void CDlgSelLayer::OnNMClickDsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_DsTree.ScreenToClient(   &point); 

	HTREEITEM hItem = m_DsTree.HitTest(point, &flag);

	m_DsTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_DsTree.GetParentItem(hItem);
	if (hParentItem == m_hDSNode)
	{
		HTREEITEM hArchiveItem = m_DsTree.GetChildItem(hItem);
		HTREEITEM hFclsTypeItem = m_DsTree.GetNextSiblingItem(hArchiveItem);
		m_strSelLayerName    = m_DsTree.GetItemText(hItem);
		m_strSelLayerArchive = m_DsTree.GetItemText(hArchiveItem);
		m_strSelLayerType    = m_DsTree.GetItemText(hFclsTypeItem);
	}

	UpdateData(FALSE);
	*pResult = 0;
}

void CDlgSelLayer::OnNMDblclkDsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_DsTree.ScreenToClient(   &point); 

	HTREEITEM hItem = m_DsTree.HitTest(point, &flag);

	m_DsTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_DsTree.GetParentItem(hItem);
	if (hParentItem == m_hDSNode)
	{
		HTREEITEM hArchiveItem = m_DsTree.GetChildItem(hItem);
		HTREEITEM hFclsTypeItem = m_DsTree.GetNextSiblingItem(hArchiveItem);
		m_strSelLayerName    = m_DsTree.GetItemText(hItem);
		m_strSelLayerArchive = m_DsTree.GetItemText(hArchiveItem);
		m_strSelLayerType    = m_DsTree.GetItemText(hFclsTypeItem);

		PostMessage(WM_COMMAND,IDOK,NULL);

		return;
	}

	UpdateData(FALSE);
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CDlgSelLayer::UpdateDsTree(void)
{
	m_DsTree.SetRedraw(FALSE);
	m_DsTree.DeleteAllItems();
	m_DsTree.SetTextColor(RGB(0,0,255));

	m_DsTree.DeleteItem(m_hDSNode);

	SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
	SmtDataSource pDS = pDSMgr->GetDataSource((LPCTSTR)m_strSelDSName);
	if (pDS && pDS.Open())
	{
		if (pDS.GetLayerCount() > 0)
		{
			m_hDSNode = m_DsTree.InsertItem(pDS.GetName());

			SmtLayerInfo layerAchiveInfo;
			int nLayers = pDS.GetLayerCount();
			for (int i = 0; i < nLayers ; i++)
			{
				pDS.GetLayerInfo(layerAchiveInfo,i);
				HTREEITEM hLayerNode = m_DsTree.InsertItem(layerAchiveInfo.szName,m_hDSNode);
				m_DsTree.InsertItem(layerAchiveInfo.szArchiveName,hLayerNode);
				m_DsTree.InsertItem(SmtDataSource::GetLayerFeatureTypeName(layerAchiveInfo.unFeatureType),hLayerNode);
				m_DsTree.Expand(hLayerNode,TVE_EXPAND);
			}
		}	

		pDS.Close();
	}	

	m_DsTree.Expand(m_hDSNode,TVE_EXPAND);
	m_DsTree.SetRedraw(TRUE);
	m_DsTree.RedrawWindow();
}
