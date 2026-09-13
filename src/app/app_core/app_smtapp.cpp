#include <objbase.h>

#include "api.h"
#include "app_smtapp.h"
#include "core_exception.h"
#include "logmanager.h"
#include "stylemanager.h"
#include "datasourcemgr.h"
#include "listenermanager.h"
#include "t_iatoolmanager.h"
#include "module_manager.h"
#include "pluginmanager.h"
#include "mapservicemgr.h"
#include "sysmanager.h"

#include "mapmgr.h"

using namespace Smt_AM;
using namespace Smt_Sys;
using namespace Smt_IATool;
using namespace Smt_SDEDevMgr;
using namespace Smt_XCatalog;

namespace Smt_App
{
	namespace {
	bool path_is_file(const char* path)
	{
		if (!path || !path[0]) {
			return false;
		}
		const DWORD attr = GetFileAttributesA(path);
		return attr != INVALID_FILE_ATTRIBUTES &&
			   (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}
	}  // namespace

	//////////////////////////////////////////////////////////////////////////
	SmtApp::SmtApp(void):m_bInit(false)
	{
		
	}

	SmtApp::~SmtApp(void)
	{
		;
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtApp::Init()
	{
		if( FAILED(::CoInitialize(NULL)) ) 
		{
			return false;
		} 

		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra;
		sysPra.fSmargin = 4;
		sysPra.bShowMBR = false;
		sysPra.bShowPoint = false;
		sysPra.lPointRaduis = 5;
		sysPra.l2DViewRefreshTime = 500;
		sysPra.l3DViewRefreshTime = 10;
		sysPra.str2DRenderDeviceName = "SmtGdiRenderDevice";

		pSysMgr->SetSysPra(sysPra);
		
		if (!InitLogMgr())
		{
			return false;
		}

		if (!InitStyleMgr())
		{
			return false;
		}

		if (!InitSmtDataSource())
		{
			return false;
		}

		if (!InitSmtListenerMgr())
		{
			return false;
		}

		if (!InitSmtAuxModules())
		{
			return false;
		}

		m_bInit = true;

		return true;
	}

	bool SmtApp::DelayInit()
	{
		if (!InitSmtMap())
		{
			return false;
		}

		if (!InitSmtMapService())
		{
			return false;
		}

		m_bInit = true;

		return true;
	}

	bool SmtApp::Destory()
	{
		if (!m_bInit)
			return true;
		 
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();
		pLog->LogMessage("Smart Gis is going to Exit!");

		//////////////////////////////////////////////////////////////////////////
		SmtStyleManager::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy SmtStyle resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtMapMgr::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy SMap resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtDataSourceMgr::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy DataSource resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtIAToolManager::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy IATools resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtListenerManager::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy AuxModules resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtPluginManager::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy Plugin resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtSysManager::GetSingletonPtr()->DestoryInstance();
		pLog->LogMessage("Destroy Sys resource!");

		::CoUninitialize();

		SmtLogManager::GetSingletonPtr()->DestoryInstance();

		m_bInit = false;

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtApp::InitLogMgr(void)
	{
		char fullpath[_MAX_PATH];
		char path[_MAX_PATH];
		char fileName[_MAX_PATH];
		char title[_MAX_PATH];
		char ext[_MAX_PATH];
		::GetModuleFileName(NULL,fullpath,MAX_PATH -1);
		SplitFileName(fullpath,path,fileName,title,ext);

		string strLogDir = path;
		strLogDir += "log\\";

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();

		if (pLogMgr)
		{
			pLogMgr->SetLogDir(strLogDir.c_str());

			if (pLogMgr->SetDefaultLog("SmtDefault"))
			{
				pLogMgr->GetDefaultLog()->LogMessage("Smart Gis is running!");

				return true;
			}		
		}

		return false;
	}

	bool SmtApp::InitStyleMgr(void)
	{ 
		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();

		if (NULL != pStyleMgr && NULL != pSysMgr)
		{
			SmtStyleConfig	  styleConfig = pSysMgr->GetSysStyleConfig();
			SmtSysPra		  sysPra = pSysMgr->GetSysPra();

			SmtPenDesc        stPenDesc;
			SmtBrushDesc      stBrushDesc;
			SmtAnnotationDesc stAnnoDesc;
			SmtSymbolDesc     stSymbolDesc;


			pStyleMgr->SetDefaultStyle("SmtDefault",stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle1 = pStyleMgr->CreateStyle(styleConfig.szPointStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle2 = pStyleMgr->CreateStyle(styleConfig.szLineStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle3 = pStyleMgr->CreateStyle(styleConfig.szRegionStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle4 = pStyleMgr->CreateStyle(styleConfig.szAuxStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle5 = pStyleMgr->CreateStyle(styleConfig.szDotFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle6 = pStyleMgr->CreateStyle(styleConfig.szDotFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle7 = pStyleMgr->CreateStyle(styleConfig.szLineFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle8 = pStyleMgr->CreateStyle(styleConfig.szLineFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle9 = pStyleMgr->CreateStyle(styleConfig.szRegionFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle10 = pStyleMgr->CreateStyle(styleConfig.szRegionFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);


			//////////////////////////////////////////////////////////////////////////

			pStyle1->SetStyleType(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);
			pStyle2->SetStyleType(ST_PenDesc);
			pStyle3->SetStyleType(ST_PenDesc|ST_BrushDesc);
			pStyle4->SetStyleType(ST_PenDesc);

			pStyle5->SetStyleType(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);
			pStyle6->SetStyleType(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);

			pStyle7->SetStyleType(ST_PenDesc);
			pStyle8->SetStyleType(ST_PenDesc);

			pStyle9->SetStyleType(ST_PenDesc|ST_BrushDesc);
			pStyle10->SetStyleType(ST_PenDesc|ST_BrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//1
			stPenDesc.lPenColor = RGB(255,0,0);
			pStyle1->SetPenDesc(stPenDesc);

			stBrushDesc.lBrushColor = RGB(0,255,255);
			pStyle1->SetBrushDesc(stBrushDesc);

			stSymbolDesc.lSymbolID = 0;
			stSymbolDesc.fSymbolWidth = stSymbolDesc.fSymbolHeight = 1.6;
			pStyle1->SetSymbolDesc(stSymbolDesc);

			//////////////////////////////////////////////////////////////////////////
			//2
			stPenDesc.lPenColor = RGB(0,0,255);
			pStyle2->SetPenDesc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//3
			stPenDesc.fPenWidth = 0.001;
			stPenDesc.lPenColor = RGB(255,0,0);
			pStyle3->SetPenDesc(stPenDesc);

			//stBrushDesc.brushTp = SmtBrushDesc::BT_Hatch;
			stBrushDesc.lBrushColor = RGB(77,255,0);
			pStyle3->SetBrushDesc(stBrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//4
			stPenDesc.lPenColor     = RGB(255,0,0);
			pStyle4->SetPenDesc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//5
			stPenDesc.lPenColor =  sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle5->SetPenDesc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr2;
			pStyle5->SetBrushDesc(stBrushDesc);

			stAnnoDesc.lAnnoClr = sysPra.flashPra.lClr1;
			pStyle5->SetAnnoDesc(stAnnoDesc);

			//////////////////////////////////////////////////////////////////////////
			//6
			stPenDesc.lPenColor =  sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle6->SetPenDesc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr1;
			pStyle6->SetBrushDesc(stBrushDesc);

			stAnnoDesc.lAnnoClr = sysPra.flashPra.lClr2;
			pStyle6->SetAnnoDesc(stAnnoDesc);

			//////////////////////////////////////////////////////////////////////////
			//7
			stPenDesc.lPenColor     = sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle7->SetPenDesc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//8
			stPenDesc.lPenColor     = sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle8->SetPenDesc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//9
			stPenDesc.lPenColor = sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle9->SetPenDesc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr2;
			pStyle9->SetBrushDesc(stBrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//10
			stPenDesc.lPenColor = sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle10->SetPenDesc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr1;
			pStyle10->SetBrushDesc(stBrushDesc);

			return true;
		}

		return false;
	}

	bool SmtApp::InitSmtDataSource(void)
	{
		bool bRet = true;

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr();
		if (pDSMgr)
		{
			string strAppPath = GetAppPath();

			string	strDSMFilePath = strAppPath + "sys\\smartgis.mds";
			if (path_is_file(strDSMFilePath.c_str())) {
				bRet = pDSMgr->Open(strDSMFilePath.c_str());
			}

			if (pDSMgr->GetDataSourceCount() == 0)
			{
				char szBuf[_MAX_DIR];
				GetParentDictory(strAppPath.c_str(),szBuf,1);
				strcat(szBuf,"data\\data source\\db\\sample1.gpkg");

				if (path_is_file(szBuf)) {
					char szPath[_MAX_PATH],szFileName[_MAX_PATH],szTitle[_MAX_PATH],szExt[_MAX_PATH];
					SplitFileName(szBuf,szPath,szFileName,szTitle,szExt);

					SmtDataSourceInfo info;
					info.unType = DS_DB_ADO;
					info.unProvider = PROVIDER_GPKG;
					strcpy(info.szName,szTitle);
					strcpy(info.db.szService,szPath);
					strcpy(info.db.szDBName,szFileName);
					strcpy(info.szUID,"");
					strcpy(info.szPWD,"");

					SmtDataSource *pDS = pDSMgr->CreateDataSource(info);

					pDSMgr->SetActiveDataSource(pDS);
				}
				// Empty catalog is OK: Feature Pack bootstrap has no sample GPKG.
				bRet = true;
			}
		}
		else
		{
			bRet = false;
		}

		return bRet;
	}

	bool SmtApp::InitSmtMap(void)
	{
		SmtMapMgr * pMapMgr = SmtMapMgr::GetSingletonPtr();
		if (!pMapMgr)
		{
			return false;
		}
		string	strDSMFilePath = GetAppPath() + "sys\\smartgis.mdoc";
		if (!path_is_file(strDSMFilePath.c_str()))
		{
			return true;
		}
		return pMapMgr->OpenMap(strDSMFilePath.c_str());
	}

	bool SmtApp::InitSmtMapService(void)
	{
		SmtMapServiceMgr * pMapServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
		if (!pMapServiceMgr)
		{
			return false;
		}
		string	strMSVRCfg = GetAppPath() + "sys\\smartgis.msvr";
		if (!path_is_file(strMSVRCfg.c_str()))
		{
			return true;
		}
		return pMapServiceMgr->OpenMSVRCfg(strMSVRCfg.c_str());
	}

	bool SmtApp::InitSmtListenerMgr(void)
	{
		bool bRet = true;

		return bRet;
	}

	bool SmtApp::InitSmtAuxModules(void)
	{
		bool bRet = true;

		SmtPluginManager *pPluginMgr = SmtPluginManager::GetSingletonPtr();
		if (pPluginMgr)
		{
			string strAppPath = GetAppPath();
			string strPluginPath = strAppPath + "aux module\\";
			//string strPluginPath = strAppPath +;
			pPluginMgr->LoadAllPlugin(strPluginPath.c_str());
		}

		return bRet;
	}
}