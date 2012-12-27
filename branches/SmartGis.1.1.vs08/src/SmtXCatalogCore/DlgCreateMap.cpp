// DlgCreateMap.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "DlgCreateMap.h"

#include "gis_feature.h"
#include "gis_sde.h"

using namespace Smt_GIS;
using namespace Smt_Core;
// CDlgCreateMap 对话框

IMPLEMENT_DYNAMIC(CDlgCreateMap, CDialog)

CDlgCreateMap::CDlgCreateMap(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgCreateMap::IDD, pParent)
{
	m_mapRect.lb.x = 0;
	m_mapRect.lb.y = 0;
	m_mapRect.rt.x = 500;
	m_mapRect.rt.y = 500;
}

CDlgCreateMap::~CDlgCreateMap()
{
}

void CDlgCreateMap::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MAP_NAME, m_strMapName);
	DDX_Text(pDX, IDC_EDIT_XMIN, m_mapRect.lb.x);
	DDX_Text(pDX, IDC_EDIT_YMIN, m_mapRect.lb.y);
	DDX_Text(pDX, IDC_EDIT_XMAX, m_mapRect.rt.x);
	DDX_Text(pDX, IDC_EDIT_YMAX, m_mapRect.rt.y);
}


BEGIN_MESSAGE_MAP(CDlgCreateMap, CDialog)
END_MESSAGE_MAP()


// CDlgCreateMap 消息处理程序
BOOL CDlgCreateMap::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  ÔÚ´ËÌí¼Ó¶îÍâµÄ³õÊ¼»¯
	
	return TRUE;  // return TRUE unless you set the focus to a control
	// Òì³£: OCX ÊôÐÔÒ³Ó¦·µ»Ø FALSE
}

void CDlgCreateMap::OnBnClickedOk()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	UpdateData(TRUE);
	OnOK();
}