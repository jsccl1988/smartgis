// DlgCreateLayer.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "DlgCreateLayer.h"

#include "gis_feature.h"
#include "gis_sde.h"

using namespace Smt_GIS;
using namespace Smt_Core;

// CDlgCreateLayer 对话框

IMPLEMENT_DYNAMIC(CDlgCreateLayer, CDialog)

CDlgCreateLayer::CDlgCreateLayer(CWnd* pParent /*=NULL*/)
: CDialog(CDlgCreateLayer::IDD, pParent)
, m_strLayerName(_T(""))
{
	m_lyrRect.lb.x = 0;
	m_lyrRect.lb.y = 0;
	m_lyrRect.rt.x = 500;
	m_lyrRect.rt.y = 500;
}

CDlgCreateLayer::~CDlgCreateLayer()
{

}

void CDlgCreateLayer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FCLS_TREE, m_FclsTree);
	DDX_Text(pDX, IDC_EDIT_LAYER_NAME, m_strLayerName);
	DDX_Text(pDX, IDC_EDIT_LAYER_TYPE, m_strSelFclsName);
	DDX_Text(pDX, IDC_EDIT_XMIN, m_lyrRect.lb.x);
	DDX_Text(pDX, IDC_EDIT_YMIN, m_lyrRect.lb.y);
	DDX_Text(pDX, IDC_EDIT_XMAX, m_lyrRect.rt.x);
	DDX_Text(pDX, IDC_EDIT_YMAX, m_lyrRect.rt.y);
}


BEGIN_MESSAGE_MAP(CDlgCreateLayer, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgCreateLayer::OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_FCLS_TREE, &CDlgCreateLayer::OnNMClickFclsTree)
	ON_EN_CHANGE(IDC_EDIT_LAYER_NAME, &CDlgCreateLayer::OnEnChangeEditLayerName)
	ON_EN_CHANGE(IDC_EDIT_XMIN, &CDlgCreateLayer::OnEnChangeEditXmin)
	ON_EN_CHANGE(IDC_EDIT_YMIN, &CDlgCreateLayer::OnEnChangeEditYmin)
	ON_EN_CHANGE(IDC_EDIT_XMAX, &CDlgCreateLayer::OnEnChangeEditXmax)
	ON_EN_CHANGE(IDC_EDIT_YMAX, &CDlgCreateLayer::OnEnChangeEditYmax)
END_MESSAGE_MAP()


// CDlgCreateLayer 消息处理程序
void CDlgCreateLayer::OnBnClickedOk()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	UpdateData(TRUE);
	OnOK();
}

void CDlgCreateLayer::OnNMClickFclsTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	*pResult = 0;
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_FclsTree.ScreenToClient(   &point); 

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
void CDlgCreateLayer::UpdateFclsTree(void)
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
	m_FclsTree.InsertItem("Grid要素",m_hFclsRoot);
	m_FclsTree.InsertItem("Tin要素",m_hFclsRoot);
	m_FclsTree.InsertItem("栅格数据",m_hFclsRoot);

	m_FclsTree.Expand(m_hFclsRoot,TVE_EXPAND);
	m_FclsTree.SetRedraw(TRUE);
	m_FclsTree.RedrawWindow();
}

BOOL CDlgCreateLayer::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  ÔÚ´ËÌí¼Ó¶îÍâµÄ³õÊ¼»¯
	UpdateFclsTree();
	return TRUE;  // return TRUE unless you set the focus to a control
	// Òì³£: OCX ÊôÐÔÒ³Ó¦·µ»Ø FALSE
}

UINT  CDlgCreateLayer::GetSelFcls(void)
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
	else if (m_strSelFclsName == "Grid要素")
	{
		unFcls = SmtFtGrid;
	}
	else if (m_strSelFclsName == "Tin要素")
	{
		unFcls = SmtFtTin;
	}
	else if (m_strSelFclsName == "栅格数据")
	{
		unFcls = SmtLayer_Ras;
	}

	return unFcls;
}

void CDlgCreateLayer::OnEnChangeEditLayerName()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateLayer::OnEnChangeEditXmin()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateLayer::OnEnChangeEditYmin()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateLayer::OnEnChangeEditXmax()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateLayer::OnEnChangeEditYmax()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}
