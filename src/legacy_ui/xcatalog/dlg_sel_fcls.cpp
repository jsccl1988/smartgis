// DLgSelFcls.cpp : 뿯宽뿯玽뿯施뿯亽
//

#include "stdafx.h"
#include "legacy_ui/xcatalog/xcatalog_core.h"
#include "legacy_ui/xcatalog/dlg_sel_fcls.h"
#include "sdb/feature/feature.h"

using namespace sdb;
// CDLgSelFcls 뿯宽话框

IMPLEMENT_DYNAMIC(CDLgSelFcls, CDialog)

CDLgSelFcls::CDLgSelFcls(CWnd* pParent /*=NULL*/)
: CDialog(CDLgSelFcls::IDD, pParent)
{

}

CDLgSelFcls::~CDLgSelFcls()
{
}

void CDLgSelFcls::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FCLS_TREE, m_FclsTree);
	DDX_Text(pDX, IDC_EDIT_LAYER_TYPE, m_strSelFclsName);
}


BEGIN_MESSAGE_MAP(CDLgSelFcls, CDialog)
	ON_BN_CLICKED(IDOK, &CDLgSelFcls::OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_FCLS_TREE, &CDLgSelFcls::OnNMClickFclsTree)
END_MESSAGE_MAP()


// CDLgSelFcls 뿯涽息处理程뿯庽

void CDLgSelFcls::OnBnClickedOk()
{
	// TODO: 뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½
	OnOK();
}

void CDLgSelFcls::OnNMClickFclsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: 뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½
	*pResult = 0;
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_FclsTree.ScreenToClient(&point); 

	HTREEITEM hItem = m_FclsTree.HitTest(point, &flag);

	m_FclsTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_FclsTree.GetParentItem(hItem);
	if (hParentItem == m_hFclsRoot)
	{
		m_strSelFclsName    = m_FclsTree.GetItemText(hItem);
	}

	UpdateData(FALSE);
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CDLgSelFcls::UpdateFclsTree(void)
{
	m_FclsTree.SetRedraw(FALSE);
	m_FclsTree.DeleteAllItems();
	m_FclsTree.SetTextColor(RGB(0,0,255));

	m_FclsTree.DeleteItem(m_hFclsRoot);
	m_hFclsRoot = m_FclsTree.InsertItem("뿯붿素类뿯垽");

	m_FclsTree.InsertItem("뿯炽뿯붿素",m_hFclsRoot);
	m_FclsTree.InsertItem("子뿯嚽뿯붿素",m_hFclsRoot);
	m_FclsTree.InsertItem("뿯沽뿯붿뿯붿素",m_hFclsRoot);
	m_FclsTree.InsertItem("뿯纽뿯붿素",m_hFclsRoot);
	m_FclsTree.InsertItem("붿뿯붿素",m_hFclsRoot);
	m_FclsTree.InsertItem("Mesh뿯붿素",m_hFclsRoot);

	m_FclsTree.Expand(m_hFclsRoot,TVE_EXPAND);
	m_FclsTree.SetRedraw(TRUE);
	m_FclsTree.RedrawWindow();
}

BOOL CDLgSelFcls::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½
	UpdateFclsTree();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 뿯½뿯½뿯½뿯½: OCX 뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½ FALSE
}

UINT  CDLgSelFcls::GetSelFcls(void)
{
	UINT unFcls = SmtFtUnknown;

	if (m_strSelFclsName == "뿯炽뿯붿素")
	{
		unFcls = SmtFtDot;
	}
	else if (m_strSelFclsName == "子뿯嚽뿯붿素")
	{
		unFcls = SmtFtChildImage;
	}
	else if (m_strSelFclsName == "뿯沽뿯붿뿯붿素")
	{
		unFcls = SmtFtAnno;
	}
	else if (m_strSelFclsName == "뿯纽뿯붿素")
	{
		unFcls = SmtFtCurve;
	}
	else if (m_strSelFclsName == "붿뿯붿素")
	{
		unFcls = SmtFtSurface;
	}
	else if (m_strSelFclsName == "Mesh뿯붿素")
	{
		unFcls = SmtFtGrid;
	}

	return unFcls;
}