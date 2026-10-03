#include "stdafx.h"
#include "legacy/ui/catalog/catalog_api.h"

#include "base/core/log.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatoolmanager.h"
#include "legacy/tool/abi/t_msg.h"
#include "legacy/ui/dialogs/dialogs_api.h"
#include "legacy/ui/catalog/map/catalog_map_doc.h"
#include "legacy/ui/catalog/map/mapmgr.h"
#include "legacy/ui/catalog/resource.h"
#include "legacy/plugin/runtime/auxmodule/plugin_msg.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "gis/model/feature/feature_api.h"
#include "gis/model/layer/layer.h"
#include "gis/model/map/map.h"
#include "legacy/sys/sysmanager.h"

using namespace gis;
using namespace gis;
using namespace sys;
using namespace ui;

#include "legacy/ui/catalog/map/dlg_create_layer.h"
#include "legacy/ui/catalog/map/dlg_sel_layer.h"

long LayerMgrAppend(void) {
  long lRtn = SMT_ERR_FAILURE;

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  DataSourceMgr *pDSMgr = DataSourceMgr::get_singleton_ptr();

  if (pMapMgr && pDSMgr) {
    CDlgSelLayer dlg;
    if (dlg.DoModal() == IDOK) {
      CString strDSName = dlg.GetSelDSName();
      SmtDataSource pDS = pDSMgr->get_data_source((LPCTSTR)strDSName);

      if (pDS && pDS.Open() && pDS.GetLayerCount() > 0) {
        CString strLayerName = dlg.GetSelLayerName();
        SmtLayer *pLayer = pMapMgr->GetLayer(strLayerName);
        OGRLayer *pOgr =
            pMapMgr->GetSmtMapPtr()
                ? pMapMgr->GetSmtMapPtr()->GetOgrLayer(strLayerName)
                : NULL;
        if (pLayer == NULL && pOgr == NULL) {
          SmtLayerInfo lyrInfo;
          pDS.GetLayerInfo(lyrInfo, strLayerName);

          if (lyrInfo.unFeatureType == SmtLayer_Ras) {
            pLayer = pDS.OpenRasterLayer(strLayerName);
            if (pLayer && pMapMgr->AppendLayer(pLayer)) lRtn = SMT_ERR_NONE;
          } else {
            OGRLayer *vl = pDS.OpenVectorLayer(strLayerName);
            if (vl && pMapMgr->AppendLayer(vl)) lRtn = SMT_ERR_NONE;
          }
        } else {
          CString strMessage;
          strMessage.Format("ͼ�� %s�Ѿ������� %s !", strLayerName,
                            pMapMgr->GetSmtMapPtr()->GetMapName());
          MessageBox(NULL, strMessage, "SmartGIS", MB_OK);
        }

        pDS.Close();
      }
    }
  }

  return lRtn;
}

long LayerMgrRemove(const char *szSelLayerName) {
  //  TODO: �ڴ�����������������?
  long lRtn = SMT_ERR_FAILURE;

  SmtMapMgr *pMapMgr = SmtMapMgr::get_singleton_ptr();
  if (pMapMgr) {
    bool bRet = pMapMgr->DeleteLayer(szSelLayerName);
    if (bRet) {
      lRtn = SMT_ERR_NONE;
    }
  }

  return lRtn;
}
