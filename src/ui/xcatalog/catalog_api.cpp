#include "stdafx.h"
#include "ui/xcatalog/resource.h"
#include "ui/xcatalog/catalog_api.h"
#include "ui/xcatalog/mapdocxcatalog.h"
#include "ui/xcatalog/mapmgr.h"
#include "sdb/map/map.h"
#include "sdb/layer/layer.h"
#include "base/core/api.h"
#include "sys/sysmanager.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sdb/feature/feature_api.h"
#include "base/core/logmanager.h"
#include "legacy_tool/t_iatoolmanager.h"
#include "base/core/msg.h"
#include "legacy_tool/group/defs.h"
#include "ui/gui/gui_api.h"
#include "plugin/legacy/plugin_msg.h"
#include "legacy_tool/t_msg.h"

using namespace sdb;
using namespace sdb;
using namespace sys;
using namespace ui;

#include "ui/xcatalog/dlg_sel_layer.h"
#include "ui/xcatalog/dlg_create_layer.h"

long	LayerMgrAppend(void)
{
	long lRtn = SMT_ERR_FAILURE;

	SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
	SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();

	if (pMapMgr && pDSMgr)
	{
		CDlgSelLayer dlg;
		if (dlg.DoModal() == IDOK)
		{
			CString strDSName = dlg.GetSelDSName();
			SmtDataSource pDS = pDSMgr->GetDataSource((LPCTSTR)strDSName);

			if (pDS && pDS.Open() && pDS.GetLayerCount() > 0)
			{
				CString strLayerName = dlg.GetSelLayerName();
				SmtLayer *pLayer = pMapMgr->GetLayer(strLayerName);
				OGRLayer *pOgr = pMapMgr->GetSmtMapPtr()
					? pMapMgr->GetSmtMapPtr()->GetOgrLayer(strLayerName)
					: NULL;
				if (pLayer == NULL && pOgr == NULL)
				{
					SmtLayerInfo lyrInfo;
					pDS.GetLayerInfo(lyrInfo,strLayerName);

					if (lyrInfo.unFeatureType == SmtLayer_Ras)
					{
						pLayer = pDS.OpenRasterLayer(strLayerName);
						if (pLayer && pMapMgr->AppendLayer(pLayer))	
							lRtn =  SMT_ERR_NONE;
					}
					else
					{
						OGRLayer *vl = pDS.OpenVectorLayer(strLayerName);
						if (vl && pMapMgr->AppendLayer(vl))
							lRtn = SMT_ERR_NONE;
					}
				}	
				else
				{
					CString strMessage;
					strMessage.Format("ͼ�� %s�Ѿ������� %s !",strLayerName,pMapMgr->GetSmtMapPtr()->GetMapName());
					MessageBox(NULL,strMessage,"SmartGIS",MB_OK);
				}	

				pDS.Close();
			}	
		}
	}

	return lRtn;
}

long LayerMgrRemove(const char *szSelLayerName)
{
	// TODO: �ڴ�����������������
	long lRtn = SMT_ERR_FAILURE;

	SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
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