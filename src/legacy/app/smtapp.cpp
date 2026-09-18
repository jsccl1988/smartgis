#include <objbase.h>

#include "base/core/api.h"
#include "legacy/app/smtapp.h"
#include "base/core/core_exception.h"
#include "base/core/log.h"
#include "sdb/carto/stylemanager.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sdb/datasource/gdal/gdal_driver.h"
#include "base/core/listenermanager.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "plugin/legacy/module_manager.h"
#include "base/util/plugin.h"
#include "base/files/read_file.h"
#include "sys/sysmanager.h"

#include "legacy/ui/xcatalog/mapmgr.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/t_msg.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

using namespace plugin;
using namespace sys;
using namespace tool;
using namespace sdb;
using namespace ui;

namespace app
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

	// Prefer the prefecture pack (recognizable China), then the tiny PLP stub.
	bool resolve_sample_geojson(std::string* out_path)
	{
		if (!out_path) {
			return false;
		}
		const std::string app = GetAppPath();
		const char* relative[] = {
			"china_city.gpkg",
			"china_city.geojson",
			"china_plp.geojson",
			"views_ogr_sample.geojson",
			"testing\\data\\china_city.gpkg",
			"testing\\data\\china_city.geojson",
			"testing\\data\\china_plp.geojson",
			"..\\testing\\data\\china_city.gpkg",
			"..\\testing\\data\\china_city.geojson",
			"..\\testing\\data\\china_plp.geojson",
			"..\\..\\testing\\data\\china_city.gpkg",
			"..\\..\\testing\\data\\china_plp.geojson",
			"..\\..\\..\\testing\\data\\china_city.gpkg",
			"..\\..\\..\\testing\\data\\china_plp.geojson",
		};
		for (const char* rel : relative) {
			const std::string cand = app + rel;
			if (path_is_file(cand.c_str())) {
				*out_path = cand;
				return true;
			}
		}
		return false;
	}

	// Open sample GeoJSON and register in the datasource manager when possible.
	// Prefer a direct GDALOpenEx (reliable for .geojson); fall back to SDBD
	// CreateDataSource for catalog visibility.
	GDALDataset* open_or_create_sample_geojson_ds(SmtDataSourceMgr* ds_mgr)
	{
		if (!ds_mgr) {
			return nullptr;
		}
		if (GDALDataset* existing = ds_mgr->GetDataSource("views_ogr_sample")) {
			return existing;
		}
		std::string geojson_path;
		if (!resolve_sample_geojson(&geojson_path)) {
			LOGGING(LOG_WARNING, "Sample GeoJSON not found (views_ogr_sample.geojson).");
			return nullptr;
		}

		GDALDataset* direct = static_cast<GDALDataset*>(GDALOpenEx(
			geojson_path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
			nullptr, nullptr));
		if (!direct) {
			LOGGING(LOG_ERROR, "GDALOpenEx failed for sample GeoJSON: %s",
			        geojson_path.c_str());
			return nullptr;
		}

		char szPath[_MAX_PATH] = {};
		char szFileName[_MAX_PATH] = {};
		char szTitle[_MAX_PATH] = {};
		char szExt[_MAX_PATH] = {};
		split_file_name(geojson_path.c_str(), szPath, szFileName, szTitle, szExt);

		SmtDataSourceInfo info;
		info.unType = DS_FILE_SMF;
		info.unProvider = PROVIDER_OGR_SUPPORT;
		strcpy_s(info.szName, "views_ogr_sample");
		strncpy_s(info.file.szPath, szPath, _TRUNCATE);
		strncpy_s(info.file.szFileName, szFileName, _TRUNCATE);

		GDALDataset* via_mgr = ds_mgr->CreateDataSource(info);
		if (via_mgr) {
			GDALClose(direct);
			LOGGING(LOG_INFO, "Opened sample GeoJSON datasource: %s",
			        geojson_path.c_str());
			return via_mgr;
		}

		// Keep the direct handle for AppendLayer; catalog may still show Memory.
		LOGGING(LOG_INFO,
		        "Sample GeoJSON open via GDALOpenEx (mgr register skipped): %s",
		        geojson_path.c_str());
		return direct;
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

		SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();

		SmtSysPra sysPra;
		sysPra.fSmargin = 4;
		sysPra.bShowMBR = false;
		sysPra.bShowPoint = false;
		sysPra.lPointRaduis = 5;
		sysPra.l2DViewRefreshTime = 500;
		sysPra.l3DViewRefreshTime = 10;
		sysPra.str2DRenderDeviceName = "SmtGdiRenderDevice";
		sysPra.str3DRenderDeviceName = "OpenGL";

		pSysMgr->set_sys_pra(sysPra);
		
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

		m_bInit = true;

		return true;
	}

	bool SmtApp::Destory()
	{
		if (!m_bInit)
			return true;
		LOGGING(LOG_INFO, "Smart Gis is going to Exit!");

		//////////////////////////////////////////////////////////////////////////
		SmtStyleManager::get_singleton_ptr()->destroy_instance();
		LOGGING(LOG_INFO, "Destroy SmtStyle resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtMapMgr::get_singleton_ptr()->DestoryInstance();
		LOGGING(LOG_INFO, "Destroy SMap resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtDataSourceMgr::get_singleton_ptr()->DestoryInstance();
		LOGGING(LOG_INFO, "Destroy DataSource resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtIAToolManager::get_singleton_ptr()->DestoryInstance();
		LOGGING(LOG_INFO, "Destroy IATools resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtListenerManager::get_singleton_ptr()->destroy_instance();
		LOGGING(LOG_INFO, "Destroy AuxModules resource!");

		//////////////////////////////////////////////////////////////////////////
		::base::plugin_manager::instance().unload_all();
		LOGGING(LOG_INFO, "Destroy Plugin resource!");

		//////////////////////////////////////////////////////////////////////////
		SmtSysManager::get_singleton_ptr()->destroy_instance();
		LOGGING(LOG_INFO, "Destroy Sys resource!");

		::CoUninitialize();

		m_bInit = false;

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtApp::InitLogMgr(void)
	{
		LOGGING(LOG_INFO, "Smart Gis is running!");
		return true;
	}

	bool SmtApp::InitStyleMgr(void)
	{ 
		SmtStyleManager * pStyleMgr = SmtStyleManager::get_singleton_ptr();
		SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();

		if (NULL != pStyleMgr && NULL != pSysMgr)
		{
			SmtStyleConfig	  styleConfig = pSysMgr->get_sys_style_config();
			SmtSysPra		  sysPra = pSysMgr->get_sys_pra();

			SmtPenDesc        stPenDesc;
			SmtBrushDesc      stBrushDesc;
			SmtAnnotationDesc stAnnoDesc;
			SmtSymbolDesc     stSymbolDesc;


			pStyleMgr->set_default_style("SmtDefault",stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle1 = pStyleMgr->create_style(styleConfig.szPointStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle2 = pStyleMgr->create_style(styleConfig.szLineStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle3 = pStyleMgr->create_style(styleConfig.szRegionStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle4 = pStyleMgr->create_style(styleConfig.szAuxStyle,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle5 = pStyleMgr->create_style(styleConfig.szDotFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle6 = pStyleMgr->create_style(styleConfig.szDotFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle7 = pStyleMgr->create_style(styleConfig.szLineFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle8 = pStyleMgr->create_style(styleConfig.szLineFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);

			SmtStyle *pStyle9 = pStyleMgr->create_style(styleConfig.szRegionFlashStyle1,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);
			SmtStyle *pStyle10 = pStyleMgr->create_style(styleConfig.szRegionFlashStyle2,stPenDesc,stBrushDesc,stAnnoDesc,stSymbolDesc);


			//////////////////////////////////////////////////////////////////////////

			pStyle1->set_style_type(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);
			pStyle2->set_style_type(ST_PenDesc);
			pStyle3->set_style_type(ST_PenDesc|ST_BrushDesc);
			pStyle4->set_style_type(ST_PenDesc);

			pStyle5->set_style_type(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);
			pStyle6->set_style_type(ST_PenDesc|ST_BrushDesc|ST_AnnoDesc|ST_SymbolDesc);

			pStyle7->set_style_type(ST_PenDesc);
			pStyle8->set_style_type(ST_PenDesc);

			pStyle9->set_style_type(ST_PenDesc|ST_BrushDesc);
			pStyle10->set_style_type(ST_PenDesc|ST_BrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//1
			stPenDesc.lPenColor = RGB(255,0,0);
			pStyle1->set_pen_desc(stPenDesc);

			stBrushDesc.lBrushColor = RGB(0,255,255);
			pStyle1->set_brush_desc(stBrushDesc);

			stSymbolDesc.lSymbolID = 0;
			stSymbolDesc.fSymbolWidth = stSymbolDesc.fSymbolHeight = 1.6;
			pStyle1->set_symbol_desc(stSymbolDesc);

			//////////////////////////////////////////////////////////////////////////
			//2
			stPenDesc.lPenColor = RGB(0,0,255);
			pStyle2->set_pen_desc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//3
			stPenDesc.fPenWidth = 0.001;
			stPenDesc.lPenColor = RGB(255,0,0);
			pStyle3->set_pen_desc(stPenDesc);

			//stBrushDesc.brushTp = SmtBrushDesc::BT_Hatch;
			stBrushDesc.lBrushColor = RGB(77,255,0);
			pStyle3->set_brush_desc(stBrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//4
			stPenDesc.lPenColor     = RGB(255,0,0);
			pStyle4->set_pen_desc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//5
			stPenDesc.lPenColor =  sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle5->set_pen_desc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr2;
			pStyle5->set_brush_desc(stBrushDesc);

			stAnnoDesc.lAnnoClr = sysPra.flashPra.lClr1;
			pStyle5->set_anno_desc(stAnnoDesc);

			//////////////////////////////////////////////////////////////////////////
			//6
			stPenDesc.lPenColor =  sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle6->set_pen_desc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr1;
			pStyle6->set_brush_desc(stBrushDesc);

			stAnnoDesc.lAnnoClr = sysPra.flashPra.lClr2;
			pStyle6->set_anno_desc(stAnnoDesc);

			//////////////////////////////////////////////////////////////////////////
			//7
			stPenDesc.lPenColor     = sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle7->set_pen_desc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//8
			stPenDesc.lPenColor     = sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle8->set_pen_desc(stPenDesc);

			//////////////////////////////////////////////////////////////////////////
			//9
			stPenDesc.lPenColor = sysPra.flashPra.lClr1;
			stPenDesc.fPenWidth = 0.002;
			pStyle9->set_pen_desc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr2;
			pStyle9->set_brush_desc(stBrushDesc);

			//////////////////////////////////////////////////////////////////////////
			//10
			stPenDesc.lPenColor = sysPra.flashPra.lClr2;
			stPenDesc.fPenWidth = 0.002;
			pStyle10->set_pen_desc(stPenDesc);

			stBrushDesc.lBrushColor = sysPra.flashPra.lClr1;
			pStyle10->set_brush_desc(stBrushDesc);

			return true;
		}

		return false;
	}

	bool SmtApp::InitSmtDataSource(void)
	{
		bool bRet = true;

		// Register GDAL early so GDAL_DATA/PROJ_LIB are set before any open.
		sdb::datasource::register_gdal_driver();

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
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
				get_parent_directory(strAppPath.c_str(),szBuf,1);
				strcat(szBuf,"data\\data source\\db\\sample1.gpkg");

				GDALDriver* gpkg_drv =
					GetGDALDriverManager()->GetDriverByName("GPKG");
				if (!gpkg_drv) {
					LOGGING(LOG_INFO, 
							"GDAL has no GPKG driver; sample GeoPackage skipped. "
							"Rebuild third_party gdal with SQLite3/GPKG enabled.");
				} else if (path_is_file(szBuf)) {
					char szPath[_MAX_PATH],szFileName[_MAX_PATH],szTitle[_MAX_PATH],szExt[_MAX_PATH];
					split_file_name(szBuf,szPath,szFileName,szTitle,szExt);

					SmtDataSourceInfo info;
					info.unType = DS_DB_ADO;
					info.unProvider = PROVIDER_GPKG;
					strcpy(info.szName,szTitle);
					strcpy(info.db.szService,szPath);
					strcpy(info.db.szDBName,szFileName);
					strcpy(info.szUID,"");
					strcpy(info.szPWD,"");

					GDALDataset* pDS = pDSMgr->CreateDataSource(info);
					if (pDS) {
						pDSMgr->SetActiveDataSource(pDS);
					} else
			LOGGING(LOG_INFO, "Failed to open sample GPKG: %s", szBuf);
				}

				// Prefer sample GeoJSON (real vectors) when GPKG is absent.
				GDALDataset* sample_ds =
					open_or_create_sample_geojson_ds(pDSMgr);
				if (sample_ds) {
					pDSMgr->SetActiveDataSource(sample_ds);
				}

				// Keep catalog usable when sample GPKG/GeoJSON is missing.
				if (pDSMgr->GetDataSourceCount() == 0) {
					SmtDataSourceInfo mem_info;
					mem_info.unType = DS_MEM;
					mem_info.unProvider = PROVIDER_MEM_VER1;
					strcpy(mem_info.szName, "Memory");
					GDALDataset* mem_ds = pDSMgr->CreateDataSource(mem_info);
					if (mem_ds) {
						pDSMgr->SetActiveDataSource(mem_ds);
			LOGGING(LOG_INFO, "Created default Memory datasource (catalog bootstrap).");
					} else
					LOGGING(LOG_INFO, 
							"Memory datasource bootstrap failed; catalog empty.");
				}
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
		SmtMapMgr * pMapMgr = SmtMapMgr::get_singleton_ptr();
		if (!pMapMgr)
		{
			return false;
		}
		string	strDSMFilePath = GetAppPath() + "sys\\smartgis.mdoc";
		if (path_is_file(strDSMFilePath.c_str()))
		{
			LOGGING(LOG_INFO, "InitSmtMap: OpenMap %s", strDSMFilePath.c_str());
			return pMapMgr->OpenMap(strDSMFilePath.c_str());
		}

		// No mdoc: bootstrap a default map with real sample geometry so
		// EDIT1/DS1 are not blank white on first launch.

		LOGGING(LOG_INFO, "InitSmtMap: NewMap begin");
		if (!pMapMgr->NewMap("Default Map")) {
			LOGGING(LOG_INFO, "NewMap(Default Map) failed.");
			return false;
		}

		SmtDataSourceMgr* pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		GDALDataset* sample_ds =
			open_or_create_sample_geojson_ds(pDSMgr);
		LOGGING(LOG_INFO, "InitSmtMap: sample_ds=%p", sample_ds);
		if (sample_ds && sample_ds->GetLayerCount() > 0) {
			int appended = 0;
			long long feats = 0;
			for (int i = 0; i < sample_ds->GetLayerCount(); ++i) {
				OGRLayer* lyr = sample_ds->GetLayer(i);
				if (!lyr) {
					continue;
				}
				LOGGING(LOG_INFO, "InitSmtMap: AppendLayer begin lyr=%p name=%s",
						lyr, lyr->GetName() ? lyr->GetName() : "(unnamed)");
				if (pMapMgr->AppendLayer(lyr)) {
					++appended;
					feats += lyr->GetFeatureCount(1);
				}
			}
			if (appended > 0) {
				LOGGING(LOG_INFO, "InitSmtMap: Update2DXView begin");
				pMapMgr->Update2DXView();
				SmtListenerMsg param;
				param.hSrcWnd = NULL;
				LOGGING(LOG_INFO, "InitSmtMap: ZOOMRESTORE begin");
				post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
								 SMT_MSG_KEY(GT_MSG_VIEW_ZOOMRESTORE, NULL),
								 param);
				LOGGING(LOG_INFO,
						"Default map bootstrapped with %d layers (%lld features).",
						appended, feats);
				return true;
			}
		}
			LOGGING(LOG_INFO, "Default map created without vector layer (sample GeoJSON missing).");
		pMapMgr->Update2DXView();
		return true;
	}

	bool SmtApp::InitSmtListenerMgr(void)
	{
		bool bRet = true;

		return bRet;
	}

	bool SmtApp::InitSmtAuxModules(void)
	{
		const ::base::path plugin_dir(::base::self_path() + "\\plugin\\");
		for (const auto& file : ::base::list_files(plugin_dir, ".am")) {
			auto* plug = ::base::plugin_manager::instance().load_plugin(
			    file.stem().string(), file);
			if (plug) {
				plug->start();
			}
		}
		return true;
	}
}