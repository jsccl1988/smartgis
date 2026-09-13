// dlg_create_map.cpp : 뿯宽뿯玽뿯施뿯亽
//

#include "stdafx.h"
#include "ui/xcatalog/xcatalog_core.h"
#include "ui/xcatalog/dlg_create_map.h"

#include "sdb/feature/feature.h"
#include "sdb/layer/layer.h"

using namespace sdb;
using namespace base;
// CDlgCreateMap 뿯宽话框

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


// CDlgCreateMap 뿯涽息处理程뿯庽
BOOL CDlgCreateMap::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½
	
	return TRUE;  // return TRUE unless you set the focus to a control
	// 뿯½뿯½뿯½뿯½: OCX 뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½ FALSE
}

void CDlgCreateMap::OnBnClickedOk()
{
	// TODO: 뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½뿯½
	UpdateData(TRUE);
	OnOK();
}