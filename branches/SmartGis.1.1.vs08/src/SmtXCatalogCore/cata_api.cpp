#include "stdafx.h"
#include "Resource.h"
#include "cata_api.h"
#include "cata_mapdocxcatalog.h"
#include "cata_mapmgr.h"
#include "gis_map.h"
#include "gis_sde.h"
#include "smt_api.h"
#include "sys_sysmanager.h"
#include "sde_datasourcemgr.h"
#include "gis_api.h"
#include "smt_logmanager.h"
#include "t_iatoolmanager.h"
#include "smt_msg.h"
#include "gt_defs.h"
#include "smt_gui_api.h"
#include "am_msg.h"
#include "t_msg.h"

using namespace Smt_SDEDevMgr;
using namespace Smt_GIS;
using namespace Smt_Sys;
using namespace Smt_XCatalog;

#include "DlgSelLayer.h"
#include "DlgCreateLayer.h"

long	LayerMgrAppend(void)
{
	long lRtn = SMT_ERR_FAILURE;

	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr();

	if (pMapMgr && pDSMgr)
	{
		CDlgSelLayer dlg;
		if (dlg.DoModal() == IDOK)
		{
			CString strDSName = dlg.GetSelDSName();
			SmtDataSource *pDS = pDSMgr->GetDataSource((LPCTSTR)strDSName);

			if (NULL != pDS && pDS->Open() && pDS->GetLayerCount() > 0)
			{
				CString strLayerName = dlg.GetSelLayerName();
				SmtLayer *pLayer = NULL;
				pLayer = pMapMgr->GetLayer(strLayerName);
				if (pLayer == NULL)
				{
					SmtLayerInfo lyrInfo;
					pDS->GetLayerInfo(lyrInfo,strLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
						pLayer = pDS->OpenRasterLayer(strLayerName);
					else 
						pLayer = pDS->OpenVectorLayer(strLayerName);	

					if (pLayer && pMapMgr->AppendLayer(pLayer))	
						lRtn =  SMT_ERR_NONE;
				}	
				else
				{
					CString strMessage;
					strMessage.Format("图层 %s已经添加至 %s !",strLayerName,pMapMgr->GetSmtMapPtr()->GetMapName());
					MessageBox(NULL,strMessage,"SmartGIS",MB_OK);
				}	

				pDS->Close();
			}	
		}
	}

	return lRtn;
}

long LayerMgrRemove(const char *szSelLayerName)
{
	// TODO: 在此添加命令处理程序代码
	long lRtn = SMT_ERR_FAILURE;

	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	if (pMapMgr)
	{
		bool bRet = pMapMgr->DeleteLayer(szSelLayerName);
		if (bRet)
		{
			lRtn = SMT_ERR_NONE;
		}
	}

	return lRtn;
}