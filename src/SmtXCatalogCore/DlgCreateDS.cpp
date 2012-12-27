// DlgCreateDS.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "DlgCreateDS.h"


#include "sde_datasourcemgr.h"
#include "smt_api.h"
using namespace Smt_SDEDevMgr;


// CDlgCreateDS 对话框

IMPLEMENT_DYNAMIC(CDlgCreateDS, CDialog)

CDlgCreateDS::CDlgCreateDS(CWnd* pParent /*=NULL*/)
: CDialog(CDlgCreateDS::IDD, pParent)
, m_strFilePath(_T(""))
, m_strFileName(_T(""))
, m_strService(_T("localhost"))
, m_strDBName(_T("sample"))
, m_strUserid(_T("sa"))
, m_strUserpwd(_T("123"))
, m_strDSName(_T("sample1"))
, m_unType(eDSType::DS_DB_ADO)
, m_unProvider(PROVIDER_ACCESS)
{

}

CDlgCreateDS::~CDlgCreateDS()
{
}

void CDlgCreateDS::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DSTYPE_TREE, m_typeTree);
	DDX_Text(pDX,IDC_EDIT_DS_FILE_PATH,m_strFilePath);
	DDX_Text(pDX, IDC_EDIT_DS_TYPE, m_strSelTypeName);
	DDX_Control(pDX, IDC_CMB_PROVIDER, m_cmbProvider);
	DDX_Text(pDX, IDC_EDIT_DS_DBSEVEICE, m_strService);
	DDX_Text(pDX, IDC_EDIT_DS_DB, m_strDBName);
	DDX_Text(pDX, IDC_EDIT_DS_USERID, m_strUserid);
	DDX_Text(pDX, IDC_EDIT_DS_PWD, m_strUserpwd);
	DDX_Text(pDX, IDC_EDIT_DS_NAME, m_strDSName);
}


BEGIN_MESSAGE_MAP(CDlgCreateDS, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgCreateDS::OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_DSTYPE_TREE, &CDlgCreateDS::OnNMClickDstypeTree)
	ON_BN_CLICKED(IDC_BTN_SELDB, &CDlgCreateDS::OnBnClickedBtnSeldb)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_DBSEVEICE, &CDlgCreateDS::OnEnKillfocusEditDsDbseveice)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_DB, &CDlgCreateDS::OnEnKillfocusEditDsDb)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_USERID, &CDlgCreateDS::OnEnKillfocusEditDsUserid)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_PWD, &CDlgCreateDS::OnEnKillfocusEditDsPwd)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_NAME, &CDlgCreateDS::OnEnKillfocusEditDsName)
	ON_CBN_SELCHANGE(IDC_CMB_PROVIDER, &CDlgCreateDS::OnCbnSelchangeCmbProvider)
	ON_NOTIFY(TVN_SELCHANGED, IDC_DSTYPE_TREE, &CDlgCreateDS::OnTvnSelchangedDstypeTree)
	ON_BN_CLICKED(IDC_BTN_SELPATH, &CDlgCreateDS::OnBnClickedBtnSelpath)
	ON_EN_KILLFOCUS(IDC_EDIT_DS_FILE_PATH, &CDlgCreateDS::OnEnKillfocusEditDsFilePath)
END_MESSAGE_MAP()


// CDlgCreateDS 消息处理程序

void CDlgCreateDS::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);

	m_dsInfo.unType = m_unType;
	m_dsInfo.unProvider = m_unProvider;
	strcpy(m_dsInfo.szName,(LPCTSTR)m_strDSName);
	strcpy(m_dsInfo.szUID,(LPCTSTR)m_strUserid);
	strcpy(m_dsInfo.szPWD,(LPCTSTR)m_strUserpwd);
	
	switch (m_unType)
	{
	case eDSType::DS_DB_ADO:
		{
			strcpy(m_dsInfo.db.szService,(LPCTSTR)m_strService);
			strcpy(m_dsInfo.db.szDBName,(LPCTSTR)m_strDBName);
		}
		break;
	case eDSType::DS_FILE_SMF:
		{
			strcpy(m_dsInfo.file.szPath,(LPCTSTR)m_strFilePath);	
			strcpy(m_dsInfo.file.szFileName,(LPCTSTR)m_strFileName);	
		}
		break;
	}
	
	OnOK();
}

void CDlgCreateDS::OnNMClickDstypeTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	CPoint   point; 
	UINT     flag;
	GetCursorPos(&point); 
	m_typeTree.ScreenToClient(&point); 

	HTREEITEM hItem = m_typeTree.HitTest(point, &flag);

	m_typeTree.SelectItem(hItem);

	HTREEITEM hParentItem = m_typeTree.GetParentItem(hItem);
	if (hParentItem == m_hTypeRoot)
	{
		m_strSelTypeName  = m_typeTree.GetItemText(hItem);
		if (m_strSelTypeName == "数据库")
		{
			m_unType = eDSType::DS_DB_ADO;
		}
		else if (m_strSelTypeName == "文件")
		{
			m_unType = eDSType::DS_FILE_SMF;
		}

		UpdateProviderCmb();

		if (m_strSelTypeName == "文件")
		{
			GetDlgItem(IDC_EDIT_DS_FILE_PATH)->EnableWindow(TRUE);
			GetDlgItem(IDC_BTN_SELPATH)->EnableWindow(TRUE);

			GetDlgItem(IDC_EDIT_DS_DBSEVEICE)->EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT_DS_DB)->EnableWindow(FALSE);
			GetDlgItem(IDC_BTN_SELDB)->EnableWindow(FALSE);
			 m_strDSName = "sample3";
		}
		else
		{
			GetDlgItem(IDC_EDIT_DS_FILE_PATH)->EnableWindow(FALSE);
			GetDlgItem(IDC_BTN_SELPATH)->EnableWindow(FALSE);

			GetDlgItem(IDC_EDIT_DS_DBSEVEICE)->EnableWindow(TRUE);
			GetDlgItem(IDC_EDIT_DS_DB)->EnableWindow(TRUE);
			GetDlgItem(IDC_BTN_SELDB)->EnableWindow(TRUE);

			 m_strDSName = "sample2";
		}
	}

	UpdateData(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CDlgCreateDS::UpdateDSTypeTree(void)
{
	m_typeTree.SetRedraw(FALSE);
	m_typeTree.DeleteAllItems();
	m_typeTree.SetTextColor(RGB(0,0,255));

	m_typeTree.DeleteItem(m_hTypeRoot);
	m_hTypeRoot = m_typeTree.InsertItem("数据源类型");

	m_typeTree.InsertItem("数据库",m_hTypeRoot);
	m_typeTree.InsertItem("文件",m_hTypeRoot);

	m_typeTree.Expand(m_hTypeRoot,TVE_EXPAND);
	m_typeTree.SetRedraw(TRUE);
	m_typeTree.RedrawWindow();
}

void CDlgCreateDS::UpdateProviderCmb()
{
	m_cmbProvider.ResetContent();
	switch (m_unType)
	{
	case eDSType::DS_DB_ADO:
		{
			m_cmbProvider.AddString("Access");
			m_cmbProvider.AddString("SQL Server");
		}
		break;
	case eDSType::DS_FILE_SMF:
		{
			m_cmbProvider.AddString("Shape");
			m_cmbProvider.AddString("OGR Support");
		}
		break;
	}
	
	m_cmbProvider.SetCurSel(0);
	OnCbnSelchangeCmbProvider();
}

BOOL CDlgCreateDS::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	UpdateDSTypeTree();

	UpdateProviderCmb();

	GetDlgItem(IDC_EDIT_DS_FILE_PATH)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_SELPATH)->EnableWindow(FALSE);

	GetDlgItem(IDC_EDIT_DS_DBSEVEICE)->EnableWindow(TRUE);
	GetDlgItem(IDC_EDIT_DS_DB)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_SELDB)->EnableWindow(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CDlgCreateDS::OnBnClickedBtnSelpath()
{
	// TODO: 在此添加控件通知处理程序代码
	static char szFilter[256];
	sprintf(szFilter,"Data Files (*.%s)|*.%s|All Files (*.*)|*.*||","smf","smf");
	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "你没有选择要打开的文件!" ) ;
		return;
	}

	char szPath[_MAX_PATH];
	char szFileName[_MAX_PATH];
	char szTitle[_MAX_PATH];
	char szExt[_MAX_PATH];

	SplitFileName(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);

	
	m_strFilePath = szPath;
	m_strFileName = szTitle;
	
	UpdateData(FALSE);
}


void CDlgCreateDS::OnBnClickedBtnSeldb()
{
	// TODO: 在此添加控件通知处理程序代码
	static char szFilter[256];
	sprintf(szFilter,"Data Files (*.%s)|*.%s|All Files (*.*)|*.*||","mdb","mdb");
	
	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "你没有选择要打开的文件!" ) ;
		return;
	}

	char szPath[_MAX_PATH];
	char szFileName[_MAX_PATH];
	char szTitle[_MAX_PATH];
	char szExt[_MAX_PATH];

	SplitFileName(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);

	if (m_unProvider == PROVIDER_ACCESS)
	{
		m_strService = szPath;
		m_strDBName =  szTitle;
	}
	
	UpdateData(FALSE);
}

void CDlgCreateDS::OnEnKillfocusEditDsDbseveice()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnEnKillfocusEditDsDb()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnEnKillfocusEditDsUserid()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnEnKillfocusEditDsPwd()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnEnKillfocusEditDsName()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnEnKillfocusEditDsFilePath()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgCreateDS::OnCbnSelchangeCmbProvider()
{
	// TODO: 在此添加控件通知处理程序代码
	m_unProvider = 0;
	CString strProvider;
	m_cmbProvider.GetLBText(m_cmbProvider.GetCurSel(),strProvider); 
	switch (m_unType)
	{
	case eDSType::DS_DB_ADO:
		{
			if (strProvider == "Access")
			{
				m_unProvider = PROVIDER_ACCESS;
				GetDlgItem(IDC_BTN_SELDB)->EnableWindow(TRUE);

				m_strService = "";
				m_strDBName	 = "";
				m_strUserid	 = "";
				m_strUserpwd = "";
			}
			else if (strProvider == "SQL Server")
			{
				m_unProvider = PROVIDER_SQLSERVER;
				GetDlgItem(IDC_BTN_SELDB)->EnableWindow(FALSE);

				m_strService = "localhost";
				m_strDBName	 = "sample";
				m_strUserid	 = "sa";
				m_strUserpwd = "123";
			}
		}
		break;
	case eDSType::DS_FILE_SMF:
		{
			if (strProvider == "Shape")
			{
				m_unProvider = PROVIDER_SHAPE;

				m_strService = "";
				m_strDBName	 = "";
				m_strUserid	 = "";
				m_strUserpwd = "";
			}
			else if (strProvider == "OGR Support")
			{
				m_unProvider = PROVIDER_OGR_SUPPORT;

				m_strService = "";
				m_strDBName	 = "";
				m_strUserid	 = "";
				m_strUserpwd = "";
			}
		}
		break;
	}

	UpdateData(FALSE);
}

void CDlgCreateDS::OnTvnSelchangedDstypeTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}



