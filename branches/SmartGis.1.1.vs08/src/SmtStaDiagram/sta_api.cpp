#include "stdafx.h"
#include "sta_api.h"

#include "Dlg2DXChartView.h"

long SmtPlot(const vPoints &points,const char * szTitle,const char * szPanelTitle,
			 const char * szXTitle,const char * szXUnit,
			 const char * szYTitle,const char * szYUnit)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CDlg2DXChartView dlg;

	dlg.m_chart.SetTitle(szTitle);
	dlg.m_chart.SetPanelTitle(szPanelTitle);
	dlg.m_chart.SetXAxis(szXTitle,szXUnit);
	dlg.m_chart.SetYAxis(szYTitle,szYUnit);
	dlg.m_chart.SetPoints(points);

	if (dlg.DoModal() == IDOK)
	{
		return SMT_ERR_NONE;
	}

	return SMT_ERR_FAILURE;
}