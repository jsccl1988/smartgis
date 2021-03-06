// DLgSelFcls.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "DLgSelFcls.h"
#include "gis_feature.h"

using namespace Smt_GIS;
// CDLgSelFcls 对话框

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


// CDLgSelFcls 消息处理程序

void CDLgSelFcls::OnBnClickedOk()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	OnOK();
}

void CDLgSelFcls::OnNMClickFclsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
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
	m_hFclsRoot = m_FclsTree.InsertItem("要素类型");

	m_FclsTree.InsertItem("点要素",m_hFclsRoot);
	m_FclsTree.InsertItem("子图要素",m_hFclsRoot);
	m_FclsTree.InsertItem("注记要素",m_hFclsRoot);
	m_FclsTree.InsertItem("线要素",m_hFclsRoot);
	m_FclsTree.InsertItem("面要素",m_hFclsRoot);
	m_FclsTree.InsertItem("Mesh要素",m_hFclsRoot);

	m_FclsTree.Expand(m_hFclsRoot,TVE_EXPAND);
	m_FclsTree.SetRedraw(TRUE);
	m_FclsTree.RedrawWindow();
}

BOOL CDLgSelFcls::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  ÔÚ´ËÌí¼Ó¶îÍâµÄ³õÊ¼»¯
	UpdateFclsTree();
	return TRUE;  // return TRUE unless you set the focus to a control
	// Òì³£: OCX ÊôÐÔÒ³Ó¦·µ»Ø FALSE
}

UINT  CDLgSelFcls::GetSelFcls(void)
{
	UINT unFcls = SmtFtUnknown;

	if (m_strSelFclsName == "点要素")
	{
		unFcls = SmtFtDot;
	}
	else if (m_strSelFclsName == "子图要素")
	{
		unFcls = SmtFtChildImage;
	}
	else if (m_strSelFclsName == "注记要素")
	{
		unFcls = SmtFtAnno;
	}
	else if (m_strSelFclsName == "线要素")
	{
		unFcls = SmtFtCurve;
	}
	else if (m_strSelFclsName == "面要素")
	{
		unFcls = SmtFtSurface;
	}
	else if (m_strSelFclsName == "Mesh要素")
	{
		unFcls = SmtFtGrid;
	}

	return unFcls;
}