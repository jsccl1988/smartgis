#include "stdafx.h"
#include "plugin/model3d/model3d_creater_plugin.h"

#include <cstring>

#include "algorithm/geo/geometry.h"
#include "base/core/api.h"
#include "plugin/dem/grid_loader.h"
#include "plugin/dem/tin_loader.h"
#include "plugin/legacy_cmd.h"
#include "plugin/model3d/model_3d_creater.h"
#include "plugin/plugin_msg.h"
#include "render/model3d/2dgeoobject.h"
#include "render/model3d/3dgeoobject.h"
#include "render/model3d/sphere.h"
#include "render/model3d/water.h"
#include "render/pointcloud/pointcloud.h"
#include "render/terrain/terrain.h"
#include "sys/sysmanager.h"
#include "tool/group/defs.h"
#include "tool/t_iatoolmanager.h"
#include "tool/t_msg.h"
#include "ui/gui/gui_api.h"
#include "ui/xcatalog/mapmgr.h"
#include "ui/xcatalog/scenemgr.h"

using namespace render;
using namespace sdb;
using namespace sys;
using namespace plugin;
using namespace ui;

const string CST_STR_3DMODELCREATER_PLUGIN_NAME = "��ά����";
Smt3DModelCreaterPlugin *g_p3DModelCreaterPlugin = NULL;

#define AM_MSG_CMD_3DMODELCREATER_BEGIN (SMT_MSG_USER_BEGIN)
#define SMT_MSG_3DMODELCREATER_1 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 1)
#define SMT_MSG_3DMODELCREATER_2 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 2)
#define SMT_MSG_3DMODELCREATER_3 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 3)
#define SMT_MSG_3DMODELCREATER_4 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 4)
#define SMT_MSG_3DMODELCREATER_5 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 5)
#define SMT_MSG_3DMODELCREATER_6 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 6)
#define SMT_MSG_3DMODELCREATER_7 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 7)
#define SMT_MSG_3DMODELCREATER_8 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 8)
#define SMT_MSG_3DMODELCREATER_9 (AM_MSG_CMD_3DMODELCREATER_BEGIN + 9)

#define AM_MSG_CMD_3DMODELCREATER_END (AM_MSG_CMD_3DMODELCREATER_BEGIN + 50)

static_assert(SMT_MSG_3DMODELCREATER_1 == plugin::kAmMsgModel3dAddPointcloud);
static_assert(SMT_MSG_3DMODELCREATER_2 == plugin::kAmMsgModel3dSphere);
static_assert(SMT_MSG_3DMODELCREATER_9 == plugin::kAmMsgModel3dLayerPolygons);

extern "C" {
int __declspec(dllexport) GetPluginVersion(void) { return 1; }

void __declspec(dllexport) StartPlugin(void) {
  g_p3DModelCreaterPlugin = new Smt3DModelCreaterPlugin();

  if (g_p3DModelCreaterPlugin) {
    g_p3DModelCreaterPlugin->Init();
  }
}

void __declspec(dllexport) StopPlugin(void) {
  if (g_p3DModelCreaterPlugin) {
    g_p3DModelCreaterPlugin->Destroy();
  }

  SMT_SAFE_DELETE(g_p3DModelCreaterPlugin);
}
}

Smt3DModelCreaterPlugin::Smt3DModelCreaterPlugin(void) {
  set_name(CST_STR_3DMODELCREATER_PLUGIN_NAME.c_str());
}

Smt3DModelCreaterPlugin::~Smt3DModelCreaterPlugin(void) { ; }

int Smt3DModelCreaterPlugin::Init(void) {
  SmtAuxModule::Init();

  append_func_items("���ӵ���ģ��", SMT_MSG_3DMODELCREATER_1,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("������ģ��", SMT_MSG_3DMODELCREATER_2,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("����ˮ��ģ��", SMT_MSG_3DMODELCREATER_3,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("���ӵ���ģ��GRID", SMT_MSG_3DMODELCREATER_4,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("���ӵ���ģ��TIN", SMT_MSG_3DMODELCREATER_5,
                    FIM_3DMFMENU | FIM_AUXMODULEBOX);
  append_func_items("����TIN", SMT_MSG_3DMODELCREATER_6, FIM_AUXMODULEBOX);

  append_func_items("��ά��ͼ��->��άչʾ", SMT_MSG_3DMODELCREATER_7,
                    FIM_AUXMODULEBOX);
  append_func_items("��ά��ͼ��->��άչʾ", SMT_MSG_3DMODELCREATER_8,
                    FIM_AUXMODULEBOX);
  append_func_items("��ά��ͼ��->��άչʾ", SMT_MSG_3DMODELCREATER_9,
                    FIM_AUXMODULEBOX);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int Smt3DModelCreaterPlugin::Destroy(void) { return SmtAuxModule::Destroy(); }

//////////////////////////////////////////////////////////////////////////
int Smt3DModelCreaterPlugin::notify(long lMsg, SmtListenerMsg &param) {
  (void)param;
  const char *id = plugin::command_id_from_am_msg(lMsg);
  long cmd = lMsg;
  if (id && std::strcmp(id, "model3d.add_pointcloud") == 0)
    cmd = SMT_MSG_3DMODELCREATER_1;
  else if (id && std::strcmp(id, "model3d.add_sphere") == 0)
    cmd = SMT_MSG_3DMODELCREATER_2;
  else if (id && std::strcmp(id, "model3d.add_water") == 0)
    cmd = SMT_MSG_3DMODELCREATER_3;
  else if (id && std::strcmp(id, "model3d.add_terrain_grid") == 0)
    cmd = SMT_MSG_3DMODELCREATER_4;
  else if (id && std::strcmp(id, "model3d.add_terrain_tin") == 0)
    cmd = SMT_MSG_3DMODELCREATER_5;
  else if (id && std::strcmp(id, "model3d.create_tin") == 0)
    cmd = SMT_MSG_3DMODELCREATER_6;
  else if (id && std::strcmp(id, "model3d.layer_points_to_3d") == 0)
    cmd = SMT_MSG_3DMODELCREATER_7;
  else if (id && std::strcmp(id, "model3d.layer_lines_to_3d") == 0)
    cmd = SMT_MSG_3DMODELCREATER_8;
  else if (id && std::strcmp(id, "model3d.layer_polygons_to_3d") == 0)
    cmd = SMT_MSG_3DMODELCREATER_9;
  if (cmd < AM_MSG_CMD_3DMODELCREATER_BEGIN ||
      cmd > AM_MSG_CMD_3DMODELCREATER_END)
    return SMT_ERR_NONE;

  SmtSceneMgr *pSceneMgr = SmtSceneMgr::get_singleton_ptr();
  SmtScene *pScene = pSceneMgr->GetScenePtr();
  if (NULL == pScene) return SMT_ERR_FAILURE;

  LP3DRENDERDEVICE p3DRenderDevice = pScene->Get3DRenderDevice();

  switch (cmd) {
    case SMT_MSG_3DMODELCREATER_1: {
      static char BASED_CODE szFilter[] =
          "Data Files (*.txt)|*.txt|All Files (*.*)|*.*||";

      CFileDialog dlg(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                      szFilter, NULL);

      if (dlg.DoModal() == IDCANCEL) {
        AfxMessageBox("��û��ѡ��Ҫ�򿪵��ļ�!");
        return false;
      }

      SmtMaterial matMaterial;
      matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
      matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
      matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
      matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
      matMaterial.SetShininessValue(20);

      Smt3DPointCloud *pPointCloud;
      pPointCloud = new Smt3DPointCloud();

      if (pPointCloud->Read3DPointCloud(dlg.GetPathName())) {
        Vector3 origin(0, 0, 0);
        if (pPointCloud->Init(origin, matMaterial) == SMT_ERR_NONE &&
            pPointCloud->Create(p3DRenderDevice) == SMT_ERR_NONE) {
          pPointCloud->SetShowOctNodeBox(true);
          pSceneMgr->Add3DObject(pPointCloud);
        }
      }

      SetActive();
    } break;
    case SMT_MSG_3DMODELCREATER_2: {
      // ���Ӳ�����
      SmtMaterial matMaterial;
      matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
      matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
      matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
      matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
      matMaterial.SetShininessValue(20);
      SmtSphere *pShere;
      pShere = new SmtSphere(20, 90);
      pShere->SetXScale(1);
      pShere->SetYScale(1);
      pShere->SetZScale(1);

      Vector3 origin(0, 0, 0);
      if (pShere->Init(origin, matMaterial, "earth") == SMT_ERR_NONE &&
          pShere->Create(p3DRenderDevice) == SMT_ERR_NONE) {
        pSceneMgr->Add3DObject(pShere);
      }

      SetActive();
    } break;
    case SMT_MSG_3DMODELCREATER_3: {
      // ����ˮ��
      SmtMaterial matMaterial;

      matMaterial.SetAmbientValue(SmtColor(1., 1., 0., 1.0));
      matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
      matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
      matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
      matMaterial.SetShininessValue(20);

      SmtWater *pWater;
      pWater = new SmtWater();
      pWater->SetXScale(4);
      pWater->SetYScale(1);
      pWater->SetZScale(4);

      Vector3 origin(30, 30, 30);
      if (pWater->Init(origin, matMaterial, "seawater") == SMT_ERR_NONE &&
          pWater->Create(p3DRenderDevice) == SMT_ERR_NONE) {
        pSceneMgr->Add3DObject(pWater);
      }

      SetActive();
    } break;
    case SMT_MSG_3DMODELCREATER_4: {
      // ���ӵ���ģ��
      SmtMaterial matMaterial;
      matMaterial.SetAmbientValue(SmtColor(1., 1., 0., 1.0));
      matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
      matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
      matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
      matMaterial.SetShininessValue(0);

      string strAppPath = get_app_path();
      char szFilePath[TEMP_BUFFER_SIZE];
      sprintf(szFilePath, "%s%s", strAppPath.c_str(),
              "rs\\terrain\\ground.bmp");

      Smt3DSurface gridSurf;
      GridLoadOptions gridOpt;
      SmtTerrain *pTerrain = new SmtTerrain;

      pTerrain->SetClrType(2);
      pTerrain->SetXScale(1);
      pTerrain->SetYScale(1);
      pTerrain->SetZScale(1);

      Vector3 origin(30, 30, 30);
      if (SMT_ERR_NONE == load_heightmap_grid(szFilePath, gridOpt, &gridSurf) &&
          SMT_ERR_NONE == pTerrain->Init(origin, matMaterial, "terrain") &&
          SMT_ERR_NONE == pTerrain->SetTerrainSurf(&gridSurf) &&
          SMT_ERR_NONE == pTerrain->Create(p3DRenderDevice)) {
        pSceneMgr->Add3DObject(pTerrain);
      }

      SetActive();
    } break;
    case SMT_MSG_3DMODELCREATER_5: {
      // ���ӵ���ģ��
      SmtMaterial matMaterial;
      matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
      matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
      matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
      matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
      matMaterial.SetShininessValue(0);

      string strAppPath = get_app_path();
      char szFilePath[TEMP_BUFFER_SIZE];
      sprintf(szFilePath, "%s%s", strAppPath.c_str(),
              "rs\\terrain\\ground.dat");

      SmtTinFileFmt tfFmt;
      Smt3DSurface tinSurf;
      SmtTerrain *pTerrain = new SmtTerrain;

      pTerrain->SetClrType(2);
      pTerrain->SetXScale(0.05);
      pTerrain->SetYScale(0.05);
      pTerrain->SetZScale(0.05);

      tfFmt.iX = 3;
      tfFmt.iY = 4;
      tfFmt.iZ = 5;
      tfFmt.nCol = 5;
      tfFmt.nHeadSkip = 0;
      tfFmt.nLineSkip = 3;
      tfFmt.nSeparatorType = ST_COMMA;

      Vector3 origin(30, 30, 30);
      if (SMT_ERR_NONE == load_ascii_xyz_tin(szFilePath, tfFmt, 0.05f, 0.05f,
                                             0.05f, &tinSurf) &&
          SMT_ERR_NONE == pTerrain->Init(origin, matMaterial, "rbed") &&
          SMT_ERR_NONE == pTerrain->SetTerrainSurf(&tinSurf) &&
          SMT_ERR_NONE == pTerrain->Create(p3DRenderDevice)) {
        pSceneMgr->Add3DObject(pTerrain);
      }

      SetActive();
    } break;

    case SMT_MSG_3DMODELCREATER_6: {
      // ����TIN
      SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
      SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

      if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) break;

      SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
      if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtTin) {
        string strAppPath = get_app_path();
        char szFilePath[TEMP_BUFFER_SIZE];
        sprintf(szFilePath, "%s%s", strAppPath.c_str(),
                "rs\\terrain\\ground.dat");

        SmtTinFileFmt fileFmt;
        fileFmt.nLineSkip = 2;
        Smt3DSurface tinSurf;
        if (SMT_ERR_NONE ==
            load_ascii_xyz_tin(szFilePath, fileFmt, 1.f, 1.f, 1.f, &tinSurf)) {
          SmtTin oSmtTin;
          if (SMT_ERR_NONE == tinSurf.copy_to_tin(&oSmtTin)) {
            SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
            SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

            SmtFeature *pSmtFeature = new SmtFeature;

            pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtTin);
            pSmtFeature->SetStyle(styleSonfig.szPointStyle);
            pSmtFeature->SetGeometry(&oSmtTin);

            if (pSmtMapMgr->AppendFeature(pSmtFeature, false)) {
              SmtListenerMsg param;
              param.hSrcWnd = GetActiveWindow();
              ::MessageBox(::GetActiveWindow(), "���ɳɹ�!", "��ʾ", MB_OK);
              (void)plugin::command_id_from_am_msg(
                  SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, param.hSrcWnd));
              post_ia_tool_msg(
                  SMT_IATOOL_MSG_BROADCAST,
                  SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH, param.hSrcWnd), param);
            } else
              SMT_SAFE_DELETE(pSmtFeature);
          }
        }
      } else
        ::MessageBox(::GetActiveWindow(), "�뼤��TINͼ��!", "��ʾ", MB_OK);
    } break;
    case SMT_MSG_3DMODELCREATER_7: {
      SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
      SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

      if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) break;

      SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
      if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtDot) {
        Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

        if (pVLayer->GetFeatureCount() == 1) {
          pVLayer->ResetReading();
          OGRFeature *pFea = pVLayer->GetNextFeature();
          OGRGeometry *pGeom = pFea ? pFea->GetGeometryRef() : nullptr;
          if (pGeom && wkbFlatten(pGeom->getGeometryType()) == wkbMultiPoint) {
            p2DGeoObj->SetGeometry(pGeom);
          }
          OGRFeature::DestroyFeature(pFea);
        } else {
          OGRMultiPoint *pMultPoint = new OGRMultiPoint();
          pVLayer->ResetReading();
          while (OGRFeature *pFea = pVLayer->GetNextFeature()) {
            if (OGRGeometry *pGeom = pFea->GetGeometryRef()) {
              pMultPoint->addGeometry(pGeom);
            }
            OGRFeature::DestroyFeature(pFea);
          }

          p2DGeoObj->SetGeometryDirectly(pMultPoint);
        }

        //////////////////////////////////////////////////////////////////////////
        SmtMaterial matMaterial;
        matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
        matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
        matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
        matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
        matMaterial.SetShininessValue(0);
        Vector3 origin(0, 0, 0);
        if (p2DGeoObj->Init(origin, matMaterial, "") == SMT_ERR_NONE &&
            p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE) {
          pSceneMgr->Add3DObject(p2DGeoObj);
        }
      } else
        ::MessageBox(::GetActiveWindow(), "�뼤���ͼ��!", "��ʾ", MB_OK);
    } break;
    case SMT_MSG_3DMODELCREATER_8: {
      SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
      SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

      if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) break;

      SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
      if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtCurve) {
        pVLayer->ResetReading();
        while (OGRFeature *pFea = pVLayer->GetNextFeature()) {
          if (OGRGeometry *pGeom = pFea->GetGeometryRef()) {
            Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

            p2DGeoObj->SetGeometry(pGeom);

            SmtMaterial matMaterial;
            matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
            matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
            matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
            matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
            matMaterial.SetShininessValue(0);
            Vector3 origin(0, 0, 0);
            if (p2DGeoObj->Init(origin, matMaterial, "") == SMT_ERR_NONE &&
                p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE) {
              pSceneMgr->Add3DObject(p2DGeoObj);
            }
          }
          OGRFeature::DestroyFeature(pFea);
        }

        pSceneMgr->CreateOctTreeSceneMgr();
      } else
        ::MessageBox(::GetActiveWindow(), "curve layer required", "hint",
                     MB_OK);
    } break;
    case SMT_MSG_3DMODELCREATER_9: {
      SmtMapMgr *pSmtMapMgr = SmtMapMgr::get_singleton_ptr();
      SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

      if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType()) break;

      SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
      if (pVLayer && leftover_layer_feature_type(pVLayer) == SmtFtSurface) {
        pVLayer->ResetReading();
        while (OGRFeature *pFea = pVLayer->GetNextFeature()) {
          if (OGRGeometry *pGeom = pFea->GetGeometryRef()) {
            Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

            p2DGeoObj->SetGeometry(pGeom);

            SmtMaterial matMaterial;
            matMaterial.SetAmbientValue(SmtColor(1., 1., 1., 1.0));
            matMaterial.SetDiffuseValue(SmtColor(1.0, 1.0, 1.0, 1.0));
            matMaterial.SetSpecularValue(SmtColor(0., 0., 0., 0.0));
            matMaterial.SetEmissiveValue(SmtColor(0., 0., 0., 1.));
            matMaterial.SetShininessValue(0);
            Vector3 origin(0, 0, 0);
            if (p2DGeoObj->Init(origin, matMaterial, "") == SMT_ERR_NONE &&
                p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE) {
              pSceneMgr->Add3DObject(p2DGeoObj);
            }
          }
          OGRFeature::DestroyFeature(pFea);
        }

        pSceneMgr->CreateOctTreeSceneMgr();
      } else
        ::MessageBox(::GetActiveWindow(), "�뼤����ͼ��!", "��ʾ", MB_OK);
    } break;
  }

  return SMT_ERR_NONE;
}