// Dlg2DFeatureInfo.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "Dlg2DFeatureInfo.h"
#include "gis_attribute.h"
#include "geo_geometry.h"
#include "bl_envelope.h"
#include "smt_logmanager.h"

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


// CDlg2DFeatureInfo ��Ϣ�������

BOOL CDlg2DFeatureInfo::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ���Ӷ���ĳ�ʼ��
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
	SmtGeometry *pSmtGeom = m_pSmtFea->GetGeometryRef();
	if (pSmtGeom->GetGeometryType() == SmtGeometryType::GTPoint)
	{
		SmtPoint *pPoint = (SmtPoint *)pSmtGeom;
		Envelope env;
		pSmtGeom->GetEnvelope(&env);
		strGeomInfo.Format("  ����:%s\n  x:%f\ty:%f",pSmtGeom->GetGeometryName(),pPoint->GetX(),pPoint->GetY());
	}
	else
	{
		Envelope env;
		pSmtGeom->GetEnvelope(&env);
		strGeomInfo.Format("  ����:%s\n  x min:%f\ty min:%f\n  x max:%f\ty max:%f",pSmtGeom->GetGeometryName(),env.MinX,env.MinY,env.MaxX,env.MaxY);
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

	SmtAttribute *pAtt = m_pSmtFea->GetAttributeRef();
	if (pAtt)
	{
		m_attGrid.SetRowCount(pAtt->GetFieldCount() + 1);

		GV_ITEM	item;
		item.mask = GVIF_TEXT|GVIF_FORMAT;
		item.nFormat = DT_CENTER;

		for (int i = 0; i < pAtt->GetFieldCount();i++)
		{
			SmtField *pFld =  pAtt->GetFieldPtr(i);
			if (pFld)
			{
				item.row = i+1;

				item.col = 0;
				item.strText = pFld->GetName();
				m_attGrid.SetItem(&item);

				item.col++;
				item.strText = SmtField::GetTypeName(pFld->GetType());
				m_attGrid.SetItem(&item);

				item.col++;
				item.strText = pFld->GetValueAsString();
				m_attGrid.SetItem(&item);
			}
		}
	}
}

void CDlg2DFeatureInfo::OnBnClickedOk()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	OnOK();
}
