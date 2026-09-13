// dlg_2d_feature_info.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "ui/gui/dlg_2d_feature_info.h"
#include "base/style/envelope.h"
#include "base/core/logmanager.h"

#include "ogrsf_frmts.h"

using namespace sdb;

// CDlg2DFeatureInfo �Ի���

IMPLEMENT_DYNAMIC(CDlg2DFeatureInfo, CDialog)

CDlg2DFeatureInfo::CDlg2DFeatureInfo(CWnd* pParent /*=NULL*/)
	: CDialog(CDlg2DFeatureInfo::IDD, pParent)
	,m_pSmtFea(NULL)
{

}

CDlg2DFeatureInfo::~CDlg2DFeatureInfo()
{
	m_pSmtFea = NULL;
}

void CDlg2DFeatureInfo::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX,IDC_STEXT_GEOM,m_geomInfo);
	DDX_Control(pDX, IDC_GRID_ATT,m_attGrid);
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlg2DFeatureInfo, CDialog)
	ON_BN_CLICKED(IDOK, &CDlg2DFeatureInfo::OnBnClickedOk)
END_MESSAGE_MAP()


// CDlg2DFeatureInfo ��Ϣ��������

BOOL CDlg2DFeatureInfo::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
	//return TRUE;
	
	m_attGrid.SetTextBkColor(RGB(0xFF, 0xFF, 0xE0));
	m_attGrid.SetEditable(FALSE);

	if (m_pSmtFea)
	{
		//
		UpdateGeomInfo();

		//
		InitAttGridHead();
		UpdateAttGridContent();
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlg2DFeatureInfo::UpdateGeomInfo()
{
	CString strGeomInfo;
	OGRGeometry *pSmtGeom = m_pSmtFea->getGeometryRef();
	if (!pSmtGeom) {
		return;
	}
	if (pSmtGeom->getGeometryType() == wkbPoint)
	{
		OGRPoint *pPoint = pSmtGeom->toPoint();
		OGREnvelope env;
		pSmtGeom->getEnvelope(&env);
		strGeomInfo.Format("  ����:%s\n  x:%f\ty:%f",pSmtGeom->getGeometryName(),pPoint->getX(),pPoint->getY());
	}
	else
	{
		OGREnvelope env;
		pSmtGeom->getEnvelope(&env);
		strGeomInfo.Format("  ����:%s\n  x min:%f\ty min:%f\n  x max:%f\ty max:%f",pSmtGeom->getGeometryName(),env.MinX,env.MinY,env.MaxX,env.MaxY);
	}

	m_geomInfo.SetWindowText(strGeomInfo);
}

void CDlg2DFeatureInfo::InitAttGridHead()
{
	m_attGrid.DeleteAllItems();

	m_attGrid.SetColumnCount(3);
	m_attGrid.SetRowCount(1);

	m_attGrid.SetFixedRowCount(1);
	m_attGrid.SetFixedColumnCount(1);

	m_attGrid.SetColumnWidth(0,80);									//�����п� 
	m_attGrid.SetColumnWidth(1,80);									//�����п� 
	m_attGrid.SetColumnWidth(2,120);								//�����п�

	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	//��������
	item.row = 0;
	item.col = 0;
	item.strText = _T("��������");
	m_attGrid.SetItem(&item);

	//��������
	item.col++;
	item.strText = _T("��������");
	m_attGrid.SetItem(&item);


	//����ֵ
	item.col++;
	item.strText = _T("����ֵ");
	m_attGrid.SetItem(&item);

	//m_attGrid.AutoSizeColumns();
}

void CDlg2DFeatureInfo::UpdateAttGridContent()
{
	if (NULL == m_pSmtFea)
		return ;

	OGRFeature* ogr = m_pSmtFea->ogr();
	if (!ogr) {
		return;
	}
	OGRFeatureDefn* defn = ogr->GetDefnRef();
	if (!defn) {
		return;
	}

	const int field_count = defn->GetFieldCount();
	m_attGrid.SetRowCount(field_count + 1);

	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	for (int i = 0; i < field_count; i++)
	{
		OGRFieldDefn* fld = defn->GetFieldDefn(i);
		if (!fld) {
			continue;
		}
		item.row = i + 1;

		item.col = 0;
		item.strText = fld->GetNameRef();
		m_attGrid.SetItem(&item);

		item.col++;
		item.strText = OGRFieldDefn::GetFieldTypeName(fld->GetType());
		m_attGrid.SetItem(&item);

		item.col++;
		item.strText = ogr->IsFieldSet(i) ? ogr->GetFieldAsString(i) : _T("");
		m_attGrid.SetItem(&item);
	}
}

void CDlg2DFeatureInfo::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	OnOK();
}
