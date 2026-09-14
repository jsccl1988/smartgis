// dlg_sel_ds.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "legacy_ui/xcatalog/xcatalog_core.h"
#include "legacy_ui/xcatalog/dlg_sel_ds.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

using namespace sdb;


// CDlgSelDS �Ի���

IMPLEMENT_DYNAMIC(CDlgSelDS, CDialog)

CDlgSelDS::CDlgSelDS(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSelDS::IDD, pParent)
	, m_strSelDSName(_T(""))
	, m_strSelDSType(_T(""))
	, m_strSelDSProvider(_T(""))
{

}

CDlgSelDS::~CDlgSelDS()
{
}

void CDlgSelDS::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DS_TREE, m_DSTree);
	DDX_Text(pDX, IDC_EDIT_DS_NAME, m_strSelDSName);
	DDX_Text(pDX, IDC_EDIT_DS_TYPE, m_strSelDSType);
	DDX_Text(pDX, IDC_EDIT_DS_PROVIDER, m_strSelDSProvider);
}


BEGIN_MESSAGE_MAP(CDlgSelDS, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgSelDS::OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_DS_TREE, &CDlgSelDS::OnNMClickDSTree)
	ON_NOTIFY(NM_DBLCLK, IDC_DS_TREE, &CDlgSelDS::OnNMDblclkDsTree)
END_MESSAGE_MAP()


// CDlgSelDS ��Ϣ��������

void CDlgSelDS::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
	OnOK();
}

BOOL CDlgSelDS::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
	UpdateDSTree();
	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlgSelDS::OnNMClickDSTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_DSTree.ScreenToClient(   &point); 

	HTREEITEM hItem = m_DSTree.HitTest(point, &flag);

	m_DSTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_DSTree.GetParentItem(hItem);
	if (hParentItem == m_hDSRoot)
	{
		HTREEITEM hTypeItem = m_DSTree.GetChildItem(hItem);
		HTREEITEM hProviderItem = m_DSTree.GetNextSiblingItem(hTypeItem);

		m_strSelDSName    = m_DSTree.GetItemText(hItem);
		m_strSelDSType = m_DSTree.GetItemText(hTypeItem);
		m_strSelDSProvider    = m_DSTree.GetItemText(hProviderItem);
	}

	UpdateData(FALSE);
	*pResult = 0;
}

void CDlgSelDS::OnNMDblclkDsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_DSTree.ScreenToClient(   &point); 

	HTREEITEM hItem = m_DSTree.HitTest(point, &flag);

	m_DSTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_DSTree.GetParentItem(hItem);
	if (hParentItem == m_hDSRoot)
	{
		HTREEITEM hTypeItem = m_DSTree.GetChildItem(hItem);
		HTREEITEM hProviderItem = m_DSTree.GetNextSiblingItem(hTypeItem);

		m_strSelDSName		= m_DSTree.GetItemText(hItem);
		m_strSelDSType		= m_DSTree.GetItemText(hTypeItem);
		m_strSelDSProvider  = m_DSTree.GetItemText(hProviderItem);

		PostMessage(WM_COMMAND,IDOK,NULL);

		return;
	}

	UpdateData(FALSE);
	*pResult = 0;
}

void  CDlgSelDS::UpdateDSTree(void)
{
	m_DSTree.SetRedraw(FALSE);
	m_DSTree.DeleteAllItems();
	m_DSTree.SetTextColor(RGB(0,0,255));

	m_DSTree.DeleteItem(m_hDSRoot);

	m_hDSRoot = m_DSTree.InsertItem("����Դ");

	SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
	pDSMgr->MoveFirst();
	while(!pDSMgr->IsEnd())
	{
		SmtDataSource pDS =  pDSMgr->GetDataSource();
		if (pDS && pDS.Open())
		{
			CString strType,strProvider;
			SmtDataSourceInfo info;
			pDS.GetInfo(info);

			strType.Format("%d",info.unType);
			strProvider.Format("%d",info.unProvider);

			HTREEITEM hDSNode = m_DSTree.InsertItem(info.szName,m_hDSRoot);
			m_DSTree.InsertItem(strType,hDSNode);
			m_DSTree.InsertItem(strProvider,hDSNode);
			m_DSTree.Expand(hDSNode,TVE_EXPAND);

			pDS.Close();
		}

		pDSMgr->MoveNext();
	}

	m_DSTree.Expand(m_hDSRoot,TVE_EXPAND);
	m_DSTree.SetRedraw(TRUE);
	m_DSTree.RedrawWindow();
}