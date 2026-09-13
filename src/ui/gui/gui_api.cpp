#include "stdafx.h"
#include "ui/gui/gui_api.h"

#include "ui/gui/dlg_input_text.h"
#include "ui/gui/dlg_2d_feature_info.h"
#include "ui/gui/dlg_select_one.h"
#include "ui/gui/dlg_att_struct_set.h"

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

long	GUI_EXPORT	SmtAttStructEditDlg(OGRLayer* layer, int nFixField)
{
	if (NULL == layer)
		return SMT_ERR_INVALID_PARAM;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CDlgAttStructSet dlg(SmtGetActiveWnd());
	dlg.SetOgrLayer(layer, nFixField);
	if (dlg.DoModal() == IDOK)
	{
		dlg.ApplyToOgrLayer();
	}

	return SMT_ERR_NONE;
}