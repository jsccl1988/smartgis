#include "stdafx.h"
#include "SmtAM3DModelCreater.h"
#include "3dmodlecreater_plugin.h"

#include "md3d_sphere.h"
#include "md3d_water.h"
#include "md3d_terrain.h"
#include "md3d_pointcloud.h"
#include "smt_api.h"
#include "smt_gui_api.h"

#include "geo_3dapi.h"
#include "cata_mapmgr.h"
#include "sys_sysmanager.h"
#include "gt_defs.h"
#include "dem_tinloader.h"
#include "dem_gridloader.h"
#include "am_msg.h"
#include "t_msg.h"
#include "am_msg.h"
#include "t_iatoolmanager.h"
#include "gt_defs.h"
#include "md3d_2dgeoobject.h"
#include "md3d_3dgeoobject.h"
#include "cata_scenemgr.h"

using namespace Smt_3DBase;
using namespace Smt_3DModel;
using namespace Smt_GIS;
using namespace Smt_Sys;
using namespace Smt_DEM;
using namespace Smt_XCatalog;

const string								CST_STR_3DMODELCREATER_PLUGIN_NAME	= "三维对象";
Smt3DModelCreaterPlugin						*g_p3DModelCreaterPlugin = NULL;

#define  AM_MSG_CMD_3DMODELCREATER_BEGIN		(SMT_MSG_USER_BEGIN)
#define  SMT_MSG_3DMODELCREATER_1				(AM_MSG_CMD_3DMODELCREATER_BEGIN+1)
#define  SMT_MSG_3DMODELCREATER_2				(AM_MSG_CMD_3DMODELCREATER_BEGIN+2)
#define  SMT_MSG_3DMODELCREATER_3				(AM_MSG_CMD_3DMODELCREATER_BEGIN+3)
#define  SMT_MSG_3DMODELCREATER_4				(AM_MSG_CMD_3DMODELCREATER_BEGIN+4)
#define  SMT_MSG_3DMODELCREATER_5				(AM_MSG_CMD_3DMODELCREATER_BEGIN+5)
#define  SMT_MSG_3DMODELCREATER_6				(AM_MSG_CMD_3DMODELCREATER_BEGIN+6)
#define  SMT_MSG_3DMODELCREATER_7				(AM_MSG_CMD_3DMODELCREATER_BEGIN+7)
#define  SMT_MSG_3DMODELCREATER_8				(AM_MSG_CMD_3DMODELCREATER_BEGIN+8)
#define  SMT_MSG_3DMODELCREATER_9				(AM_MSG_CMD_3DMODELCREATER_BEGIN+9)

#define  AM_MSG_CMD_3DMODELCREATER_END			(AM_MSG_CMD_3DMODELCREATER_BEGIN+50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_p3DModelCreaterPlugin = new Smt3DModelCreaterPlugin();

		if (g_p3DModelCreaterPlugin)
		{
			g_p3DModelCreaterPlugin->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_p3DModelCreaterPlugin)
		{
			g_p3DModelCreaterPlugin->Destroy();
		}

		SMT_SAFE_DELETE(g_p3DModelCreaterPlugin);
	}
}

Smt3DModelCreaterPlugin::Smt3DModelCreaterPlugin(void)
{
	SetName(CST_STR_3DMODELCREATER_PLUGIN_NAME.c_str());
}

Smt3DModelCreaterPlugin::~Smt3DModelCreaterPlugin(void)
{
	;
}

int Smt3DModelCreaterPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("添加点云模型",SMT_MSG_3DMODELCREATER_1,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("添加球模型",SMT_MSG_3DMODELCREATER_2,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("添加水面模型",SMT_MSG_3DMODELCREATER_3,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("添加地形模型GRID",SMT_MSG_3DMODELCREATER_4,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("添加地形模型TIN",SMT_MSG_3DMODELCREATER_5,FIM_3DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("创建TIN",SMT_MSG_3DMODELCREATER_6,FIM_AUXMODULEBOX);

	AppendFuncItems("二维点图层->三维展示",SMT_MSG_3DMODELCREATER_7,FIM_AUXMODULEBOX);
	AppendFuncItems("二维线图层->三维展示",SMT_MSG_3DMODELCREATER_8,FIM_AUXMODULEBOX);
	AppendFuncItems("二维区图层->三维展示",SMT_MSG_3DMODELCREATER_9,FIM_AUXMODULEBOX);

	RegisterMsg();

	return SMT_ERR_NONE;
}

int Smt3DModelCreaterPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

//////////////////////////////////////////////////////////////////////////
int Smt3DModelCreaterPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	if (lMsg < AM_MSG_CMD_3DMODELCREATER_BEGIN  ||
		lMsg > AM_MSG_CMD_3DMODELCREATER_END)
		return SMT_ERR_NONE;

	SmtSceneMgr			*pSceneMgr = SmtSceneMgr::GetSingletonPtr();
	SmtScene			*pScene = pSceneMgr->GetScenePtr();
	if (NULL == pScene)
		return SMT_ERR_FAILURE;

	LP3DRENDERDEVICE	p3DRenderDevice = pScene->Get3DRenderDevice();

	switch (lMsg)
	{
	case SMT_MSG_3DMODELCREATER_1:
		{
			static char BASED_CODE szFilter[] = "Data Files (*.txt)|*.txt|All Files (*.*)|*.*||" ;

			CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

			if( dlg.DoModal() == IDCANCEL )
			{
				AfxMessageBox( "你没有选择要打开的文件!" ) ;
				return false;
			}

			SmtMaterial matMaterial;
			matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
			matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
			matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
			matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
			matMaterial.SetShininessValue(20);

			Smt3DPointCloud *pPointCloud;
			pPointCloud = new Smt3DPointCloud();

			if (pPointCloud->Read3DPointCloud(dlg.GetPathName()))
			{
				if (pPointCloud->Init(Vector3(0,0,0),matMaterial) == SMT_ERR_NONE && 
					pPointCloud->Create(p3DRenderDevice) == SMT_ERR_NONE)
				{
					pPointCloud->SetShowOctNodeBox(true);
					pSceneMgr->Add3DObject(pPointCloud);
				}
			}

			SetActive();
		}
		break;
	case SMT_MSG_3DMODELCREATER_2:
		{
			//添加测试球
			SmtMaterial matMaterial;
			matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
			matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
			matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
			matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
			matMaterial.SetShininessValue(20);
			SmtSphere *pShere;
			pShere = new SmtSphere(20,90);
			pShere->SetXScale(1);
			pShere->SetYScale(1);
			pShere->SetZScale(1);

			if (pShere->Init(Vector3(0,0,0),matMaterial,"earth") == SMT_ERR_NONE && 
				pShere->Create(p3DRenderDevice) == SMT_ERR_NONE)
			{
				pSceneMgr->Add3DObject(pShere);
			}

			SetActive();
		}
		break;
	case SMT_MSG_3DMODELCREATER_3:
		{
			//添加水面
			SmtMaterial matMaterial;

			matMaterial.SetAmbientValue(SmtColor(1.,1.,0.,1.0));
			matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
			matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
			matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
			matMaterial.SetShininessValue(20);

			SmtWater *pWater;
			pWater = new SmtWater();
			pWater->SetXScale(4);
			pWater->SetYScale(1);
			pWater->SetZScale(4);

			if (pWater->Init(Vector3(30,30,30),matMaterial,"seawater") == SMT_ERR_NONE && 
				pWater->Create(p3DRenderDevice) == SMT_ERR_NONE)
			{
				pSceneMgr->Add3DObject(pWater);
			}

			SetActive();
		}
		break;
	case SMT_MSG_3DMODELCREATER_4:
		{
			//添加地形模型
			SmtMaterial matMaterial;
			matMaterial.SetAmbientValue(SmtColor(1.,1.,0.,1.0));
			matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
			matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
			matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
			matMaterial.SetShininessValue(0);

			string strAppPath = GetAppPath();
			char szFilePath[TEMP_BUFFER_SIZE];
			sprintf(szFilePath,"%s%s",strAppPath.c_str(),"rs\\terrain\\ground.bmp");

			Smt3DGridLoader			gridLoader;
			SmtTerrain *pTerrain = new SmtTerrain;

			pTerrain->SetClrType(2);
			pTerrain->SetXScale(1);
			pTerrain->SetYScale(1);
			pTerrain->SetZScale(1);

			gridLoader.SetXScale(1);
			gridLoader.SetYScale(1);
			gridLoader.SetZScale(1);

			gridLoader.SetXStart(0);
			gridLoader.SetYStart(0);
			gridLoader.SetZStart(0);

			if (SMT_ERR_NONE == gridLoader.LoadDataFromHeightMap(szFilePath)&&
				SMT_ERR_NONE == pTerrain->Init(Vector3(30,30,30),matMaterial,"terrain")&& 
				SMT_ERR_NONE == pTerrain->SetTerrainSurf(gridLoader.GetGridSurf()) &&
				SMT_ERR_NONE == pTerrain->Create(p3DRenderDevice))
			{
				pSceneMgr->Add3DObject(pTerrain);
			}

			SetActive();
		}
		break;
	case SMT_MSG_3DMODELCREATER_5:
		{
			//添加地形模型
			SmtMaterial matMaterial;
			matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
			matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
			matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
			matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
			matMaterial.SetShininessValue(0);

			string strAppPath = GetAppPath();
			char szFilePath[TEMP_BUFFER_SIZE];
			sprintf(szFilePath,"%s%s",strAppPath.c_str(),"rs\\terrain\\ground.dat");

			Smt3DTinLoader		tinLoader;
			SmtTinFileFmt		tfFmt;
			SmtTerrain			*pTerrain = new SmtTerrain;

			pTerrain->SetClrType(2);
			pTerrain->SetXScale(0.05);
			pTerrain->SetYScale(0.05);
			pTerrain->SetZScale(0.05);

			tinLoader.SetXScale(0.05);
			tinLoader.SetYScale(0.05);
			tinLoader.SetZScale(0.05);

			tfFmt.iX = 3;
			tfFmt.iY = 4;
			tfFmt.iZ = 5;
			tfFmt.nCol = 5;
			tfFmt.nHeadSkip = 0;
			tfFmt.nLineSkip = 3;
			tfFmt.nSeparatorType = ST_COMMA;

			if (SMT_ERR_NONE == tinLoader.LoadDataFromASSII(szFilePath,tfFmt) &&
				SMT_ERR_NONE == pTerrain->Init(Vector3(30,30,30),matMaterial,"rbed")&& 
				SMT_ERR_NONE == pTerrain->SetTerrainSurf(tinLoader.GetTinSurf()) &&
				SMT_ERR_NONE == pTerrain->Create(p3DRenderDevice))
			{
				pSceneMgr->Add3DObject(pTerrain);
			}

			SetActive();
		}
		break;

	case SMT_MSG_3DMODELCREATER_6:
		{
			//创建TIN
			SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
			SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

			if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
				break;

			SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
			if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtTin)
			{
				Smt3DTinLoader	tinLoader;
				tinLoader.SetXScale(1);
				tinLoader.SetYScale(1);
				tinLoader.SetZScale(1);

				string strAppPath = GetAppPath();
				char szFilePath[TEMP_BUFFER_SIZE];
				sprintf(szFilePath,"%s%s",strAppPath.c_str(),"rs\\terrain\\ground.dat");

				SmtTinFileFmt fileFmt;
				fileFmt.nLineSkip = 2;
				if(tinLoader.LoadDataFromASSII(szFilePath,fileFmt))
				{
					SmtTin      oSmtTin;
					Smt3DSurface *pTinSurf = tinLoader.GetTinSurf();
					if (SMT_ERR_NONE == SmtCvt3DSurfTo2DTin(pTinSurf,&oSmtTin))
					{
						SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
						SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

						SmtFeature *pSmtFeature = new SmtFeature;

						pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtTin);
						pSmtFeature->SetStyle(styleSonfig.szPointStyle);
						pSmtFeature->SetGeometry(&oSmtTin);

						if (pSmtMapMgr->AppendFeature(pSmtFeature,false))
						{
							SmtListenerMsg param;
							param.hSrcWnd = GetActiveWindow();
							::MessageBox(::GetActiveWindow(), "生成成功!", "提示", MB_OK);
							SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_MSG_KEY(GT_MSG_VIEW_ZOOMREFRESH,param.hSrcWnd),param);
						}
						else
							SMT_SAFE_DELETE(pSmtFeature);
					}
				}
			}
			else
				::MessageBox(::GetActiveWindow(), "请激活TIN图层!", "提示", MB_OK);
		}
		break;
	case SMT_MSG_3DMODELCREATER_7:
		{
			SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
			SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

			if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
				break;

			SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
			if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtDot)
			{
				Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

				if (pVLayer->GetFeatureCount() == 1)
				{//有可能是 MultiPoint
					SmtFeature  *pFea = NULL;
					SmtGeometry *pGeom = NULL;
					SmtStyle	*pStyle = NULL;
					pVLayer->MoveFirst();
					if (NULL != (pFea	= pVLayer->GetFeature())&&
						NULL != (pGeom	= pFea->GetGeometryRef()) &&
						GTMultiPoint == pGeom->GetGeometryType() &&
						NULL != (pStyle = pFea->GetStyle()))
					{
						p2DGeoObj->SetGeometry(pGeom);
						p2DGeoObj->SetStyle(pStyle);
					}
				}
				else
				{//单点	
					SmtMultiPoint  *pMultPoint = new SmtMultiPoint();

					SmtFeature  *pFea = NULL;
					SmtGeometry *pGeom = NULL;
					SmtStyle	*pStyle = NULL;
					pVLayer->MoveFirst();
					while(!pVLayer->IsEnd())
					{
						if (NULL != (pFea = pVLayer->GetFeature())&&
							NULL != (pGeom = pFea->GetGeometryRef()) &&
							NULL != (pStyle = pFea->GetStyle()))
						{
							pMultPoint->AddGeometry(pGeom);
						}

						pVLayer->MoveNext();
					}

					p2DGeoObj->SetGeometryDirectly(pMultPoint);
					p2DGeoObj->SetStyle(pStyle);
				}

				//////////////////////////////////////////////////////////////////////////
				SmtMaterial matMaterial;
				matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
				matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
				matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
				matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
				matMaterial.SetShininessValue(0);
				if (p2DGeoObj->Init(Vector3(0,0,0),matMaterial,"") == SMT_ERR_NONE && 
					p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE)
				{
					pSceneMgr->Add3DObject(p2DGeoObj);
				}
			}
			else
				::MessageBox(::GetActiveWindow(), "请激活点图层!", "提示", MB_OK);
		}
		break;
	case SMT_MSG_3DMODELCREATER_8:
		{
			SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
			SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

			if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
				break;

			SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
			if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtCurve)
			{
				SmtFeature  *pFea = NULL;
				SmtGeometry *pGeom = NULL;
				SmtStyle	*pStyle = NULL;

				pVLayer->MoveFirst();
				while(!pVLayer->IsEnd())
				{
					if (NULL != (pFea = pVLayer->GetFeature())&&
						NULL != (pGeom = pFea->GetGeometryRef()) &&
						NULL != (pStyle = pFea->GetStyle()))
					{
						Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

						p2DGeoObj->SetGeometry(pGeom);
						p2DGeoObj->SetStyle(pStyle);

						SmtMaterial matMaterial;
						matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
						matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
						matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
						matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
						matMaterial.SetShininessValue(0);
						if (p2DGeoObj->Init(Vector3(0,0,0),matMaterial,"") == SMT_ERR_NONE && 
							p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE)
						{
							pSceneMgr->Add3DObject(p2DGeoObj);
						}
					}

					pVLayer->MoveNext();
				}

				pSceneMgr->CreateOctTreeSceneMgr();
			}
			else
				::MessageBox(::GetActiveWindow(), "请激活线图层!", "提示", MB_OK);
		}
		break;
	case SMT_MSG_3DMODELCREATER_9:
		{
			SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
			SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

			if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
				break;

			SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
			if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtSurface)
			{
				SmtFeature  *pFea = NULL;
				SmtGeometry *pGeom = NULL;
				SmtStyle	*pStyle = NULL;

				pVLayer->MoveFirst();
				while(!pVLayer->IsEnd())
				{
					if (NULL != (pFea = pVLayer->GetFeature())&&
						NULL != (pGeom = pFea->GetGeometryRef()) &&
						NULL != (pStyle = pFea->GetStyle()))
					{
						Smt2DGeoObject *p2DGeoObj = new Smt2DGeoObject();

						p2DGeoObj->SetGeometry(pGeom);
						p2DGeoObj->SetStyle(pStyle);

						SmtMaterial matMaterial;
						matMaterial.SetAmbientValue(SmtColor(1.,1.,1.,1.0));
						matMaterial.SetDiffuseValue(SmtColor(1.0,1.0,1.0,1.0));
						matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,0.0));
						matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.));
						matMaterial.SetShininessValue(0);
						if (p2DGeoObj->Init(Vector3(0,0,0),matMaterial,"") == SMT_ERR_NONE && 
							p2DGeoObj->Create(p3DRenderDevice) == SMT_ERR_NONE)
						{
							pSceneMgr->Add3DObject(p2DGeoObj);
						}
					}

					pVLayer->MoveNext();
				}

				pSceneMgr->CreateOctTreeSceneMgr();
			}
			else
				::MessageBox(::GetActiveWindow(), "请激活区图层!", "提示", MB_OK);
		}
		break;
	}

	return SMT_ERR_NONE;
}