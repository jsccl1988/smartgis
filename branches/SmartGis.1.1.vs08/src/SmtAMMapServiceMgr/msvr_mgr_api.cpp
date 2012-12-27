#include "stdafx.h"
#include "msvr_mgr_api.h"

#include "Dlg2DXView.h"
#include "DlgMapClient.h"

long MSVR_MGR_ViewMap(string	 strMDocFile)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CDlg2DXView dlg(strMDocFile);
	if (dlg.DoModal() == IDOK)
	{
		;
	}

	return SMT_ERR_NONE;
}

long MSVR_MGR_ViewTileMap(SmtMapService	*pMapService)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CDlgMapClient dlg(pMapService);
	if (dlg.DoModal() == IDOK)
	{
		;
	}

	return SMT_ERR_NONE;
}