// dlg_att_struct_set.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "ui/gui/dlg_att_struct_set.h"

// CDlgAttStructSet �Ի���

IMPLEMENT_DYNAMIC(CDlgAttStructSet, CDialog)

CDlgAttStructSet::CDlgAttStructSet(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgAttStructSet::IDD, pParent)
	,m_pSmtAtt(NULL)
	,m_nFixField(0)
	,m_selRow(-1)
{

}

CDlgAttStructSet::~CDlgAttStructSet()
{
	SMT_SAFE_DELETE(m_pSmtAtt);
}

void CDlgAttStructSet::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_GRID_ATT_STRUCT,m_attStruGrid);
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgAttStructSet, CDialog)
	ON_BN_CLICKED(IDOK, &CDlgAttStructSet::OnBnClickedOk)
	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_GRID_ATT_STRUCT, &CDlgAttStructSet::OnGridClickEndEdit)
	ON_NOTIFY(NM_RCLICK, IDC_GRID_ATT_STRUCT,&CDlgAttStructSet:: OnGridRClick)
	ON_COMMAND(ID_ATTSTRUCT_APPEND, &CDlgAttStructSet::OnAttstructAppend)
	ON_COMMAND(ID_ATTSTRUCT_REMOVE, &CDlgAttStructSet::OnAttstructRemove)
	ON_COMMAND(ID_ATTSTRUCT_MOVEUP, &CDlgAttStructSet::OnAttstructMoveup)
	ON_COMMAND(ID_ATTSTRUCT_MOVEDOWN, &CDlgAttStructSet::OnAttstructMovedown)
END_MESSAGE_MAP()


// CDlgAttStructSet ��Ϣ��������

void CDlgAttStructSet::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	OnOK();
}

BOOL CDlgAttStructSet::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
	//
	vector<string> vAllFldNames;
	SmtField::GetAllTypeNames(vAllFldNames);

	for (int i = 0; i < vAllFldNames.size();i++)
	{
		m_arAllFldNames.Add(vAllFldNames[i].c_str());
	}

	m_attStruGrid.SetTextBkColor(RGB(0xFF, 0xFF, 0xE0));
	m_attStruGrid.SetEditable(TRUE);

	InitAttStructGridHead();
	if (m_pSmtAtt)
	{
		UpdateAttStructGridContent();
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlgAttStructSet::InitAttStructGridHead()
{
	m_attStruGrid.DeleteAllItems();
	m_attStruGrid.SetRowCount(1);
	m_attStruGrid.SetColumnCount(3);
	m_attStruGrid.SetFixedColumnCount(1);
	
	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	//���
	item.row = 0;
	item.col = 0;
	item.strText = _T("���");
	m_attStruGrid.SetItem(&item);

	//��������
	item.col++;
	item.strText = _T("��������");
	m_attStruGrid.SetItem(&item);

	//��������
	item.col++;
	item.strText = _T("��������");
	m_attStruGrid.SetItem(&item);

	m_attStruGrid.AutoSizeColumns();
	m_attStruGrid.AutoSizeRows();
}

void CDlgAttStructSet::UpdateAttStructGridContent()
{
	if (NULL == m_pSmtAtt)
		return ;

	m_attStruGrid.SetRowCount(m_pSmtAtt->GetFieldCount()+1);
	//m_attStruGrid.SetFixedRowCount(m_nFixField +1);
	m_attStruGrid.SetFixedRowCount(1);

	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	for (int i = 0; i < m_pSmtAtt->GetFieldCount();i++)
	{
		SmtField *pFld =  m_pSmtAtt->GetFieldPtr(i);
		if (pFld)
		{
			item.row = i+1;
			//���
			item.col = 0;
			item.strText.Format("%d",i+1);
			m_attStruGrid.SetItem(&item);

			//��������
			item.col++;
			item.strText = pFld->GetName();
			m_attStruGrid.SetItem(&item);
			m_attStruGrid.SetItemState(item.row,item.col, m_attStruGrid.GetItemState(item.row,item.col) | GVIS_READONLY);

			//��������
			item.col++;
			m_attStruGrid.SetCellType(item.row,item.col, RUNTIME_CLASS(CGridCellCombo));
			CGridCellCombo* pCmb = (CGridCellCombo*)m_attStruGrid.GetCell(item.row,item.col);
			if (pCmb)
			{
				pCmb->SetOptions(m_arAllFldNames);
				pCmb->SetText(pFld->GetTypeName(pFld->GetType()));
				//m_attStruGrid.SetItemState(item.row,item.col, m_attStruGrid.GetItemState(item.row,item.col) | GVIS_READONLY);
			}
		}
	}
}

void CDlgAttStructSet::OnGridClickEndEdit(NMHDR *pNotifyStruct, LRESULT* pResult)
{
	pResult = 0;
	NM_GRIDVIEW*	pItem = (NM_GRIDVIEW*) pNotifyStruct;

	if(pItem->iRow <= 0 || pItem->iColumn <= 0) 
		return;

	if (pItem->iRow <= m_pSmtAtt->GetFieldCount())
	{
		CString strValue;
		strValue = m_attStruGrid.GetItemText(pItem->iRow,pItem->iColumn);
		SmtField *pFld = m_pSmtAtt->GetFieldPtr(pItem->iRow-1);
		if (pItem->iColumn == 1)
		{
			if (pFld)
			{
				pFld->SetName((LPCTSTR)strValue);
			}
		}
		else if (pItem->iColumn == 2)
		{
			if (pFld)
			{
				pFld->SetType(SmtField::GetType((LPCTSTR)strValue));
			}
		}
	}
	
	m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnGridRClick(NMHDR *pNotifyStruct, LRESULT* pResult)
{
	NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
	pItem->iRow, pItem->iColumn;

	if (pItem->iRow  > 0)
	{
		m_selRow = pItem->iRow;
		CMenu menuMapMgr;
		menuMapMgr.LoadMenu(IDR_MENU_ATTSTRUCT_EDIT);
		CMenu* pMenu = menuMapMgr.GetSubMenu(0);
		if (pMenu)
		{
			CPoint      menuPos;   
			GetCursorPos(&menuPos);  

			pMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,menuPos.x,menuPos.y,this);
			pMenu->Detach();
		}		
	}
}

void CDlgAttStructSet::OnAttstructAppend()
{
	// TODO: �ڴ�����������������
	CString strTmp;

	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	item.row = m_attStruGrid.GetRowCount();
	item.col = 0;
	strTmp.Format("%d",item.row+1);
	m_attStruGrid.InsertRow(strTmp);

	//��������
	item.col++;
	item.strText =  "";
	m_attStruGrid.SetItem(&item);

	//��������
	item.col++;
	m_attStruGrid.SetCellType(item.row,item.col, RUNTIME_CLASS(CGridCellCombo));
	CGridCellCombo* pCmb = (CGridCellCombo*)m_attStruGrid.GetCell(item.row,item.col);
	if (pCmb)
	{
		pCmb->SetOptions(m_arAllFldNames);
	}

	m_attStruGrid.AutoSizeColumns();
	m_attStruGrid.Invalidate();

	UpdateData(FALSE);
}

void CDlgAttStructSet::OnAttstructRemove()
{
	// TODO: �ڴ�����������������
	m_attStruGrid.DeleteRow(m_selRow);
	m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnAttstructMoveup()
{
	// TODO: �ڴ�����������������
}

void CDlgAttStructSet::OnAttstructMovedown()
{
	// TODO: �ڴ�����������������
}


//////////////////////////////////////////////////////////////////////////

void CDlgAttStructSet::SetAttStruct(SmtAttribute *pSmtAtt,int nFixField) 
{
	if (pSmtAtt)
	{
		SMT_SAFE_DELETE(m_pSmtAtt);
		m_pSmtAtt = pSmtAtt->clone();
		m_nFixField = nFixField;
	}
}

void CDlgAttStructSet::GetAttStruct(SmtAttribute *&pSmtAtt) 
{
	if (m_pSmtAtt)
		pSmtAtt = m_pSmtAtt->clone();
}
