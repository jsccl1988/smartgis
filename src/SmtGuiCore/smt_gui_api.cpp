#include "stdafx.h"
#include "smt_gui_api.h"

#include "DlgInputText.h"
#include "Dlg2DFeatureInfo.h"
#include "DlgSelectOne.h"
#include "DlgAttStructSet.h"

//////////////////////////////////////////////////////////////////////////
//mfc
CWnd *SmtGetActiveWnd(void)
{
	return CWnd::FromHandle(::GetActiveWindow());
}

//////////////////////////////////////////////////////////////////////////
long	SmtInputTextDlg(string &strText)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CDlgInputText dlg;
	if (dlg.DoModal() == IDOK)
	{
		strText = (LPCTSTR)dlg.m_strText;
	}
	return SMT_ERR_NONE;
}

long	SmtEditParamSettingDlg(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	/*CDlgEditParamSetting dlg;
	if (dlg.DoModal() == IDOK)
	{
		;
	}*/
	return SMT_ERR_NONE;
}

long	SmtSelectOneDlg(uint &unID,vector<uint> &vIDs)
{
	if (vIDs.size() < 2)
		return SMT_ERR_INVALID_PARAM;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CDlgSelectOne dlg(SmtGetActiveWnd());
	dlg.SetIDList(vIDs);
	if (dlg.DoModal() == IDOK)
	{
		unID = dlg.GetSelectedID();
	}
	return SMT_ERR_NONE;
}

long	SmtShow2DFeatureInfoDlg(SmtFeature *pSmtFea)
{
	if (NULL == pSmtFea)
		return SMT_ERR_INVALID_PARAM;
	
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CDlg2DFeatureInfo dlg(SmtGetActiveWnd());
	dlg.SetFeature(pSmtFea);
	if (dlg.DoModal() == IDOK)
	{
		;
	}

	return SMT_ERR_NONE;
}

long	SMT_EXPORT_API	SmtAttStructEditDlg(SmtAttribute *&pSmtAtt,int nFixField)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CDlgAttStructSet dlg(SmtGetActiveWnd());
	dlg.SetAttStruct(pSmtAtt,nFixField);
	if (dlg.DoModal() == IDOK)
	{
		dlg.GetAttStruct(pSmtAtt);
	}

	return SMT_ERR_NONE;
}