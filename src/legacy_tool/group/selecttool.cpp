#include "legacy_tool/group/selecttool.h"
#include "base/core/api.h"
#include "algorithm/geo/geometry.h"
#include <cstdint>
#include <vector>
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sys/sysmanager.h"
#include "legacy_tool/t_iatoolmanager.h"
#include "base/core/msg_def.h"
#include "base/core/logmanager.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "ogrsf_frmts.h"

using namespace render;
using namespace base;
using namespace sdb;
using namespace geo;
using namespace base;
using namespace sys;
using namespace sdb;

const string						CST_STR_SELECT_TOOL_NAME	= "选取";
const string						C_STR_SELECT_TOO_LOG = "SmtSelectTool";

long SmtSelectOneDlg(uint& unID, std::vector<uint>& vIDs);

namespace {

void clear_scratch(ScratchLayer& sl) {
	SmtDataSourceMgr::DestoryMemVecLayer(sl);
	sl = SmtDataSourceMgr::CreateMemVecLayer();
}

int ogr_feature_count(OGRLayer* layer) {
	if (!layer) {
		return 0;
	}
	const int n = static_cast<int>(layer->GetFeatureCount());
	return n < 0 ? 0 : n;
}

void collect_fids(OGRLayer* layer, std::vector<uint>& ids) {
	if (!layer) {
		return;
	}
	layer->ResetReading();
	while (OGRFeature* feat = layer->GetNextFeature()) {
		ids.push_back(static_cast<uint>(feat->GetFID()));
		OGRFeature::DestroyFeature(feat);
	}
}

void post_flash_data(HWND hwnd, ScratchLayer* scratch, int* fea_type) {
	SmtListenerMsg param;
	param.hSrcWnd = hwnd;
	param.wParam = WPARAM(scratch);
	param.lParam = LPARAM(fea_type);
	post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
			 SMT_MSG_KEY(GT_MSG_SET_FLASH_DATA, hwnd), param);
}

void post_flash_start(HWND hwnd) {
	SmtListenerMsg param;
	param.hSrcWnd = hwnd;
	post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
			 SMT_MSG_KEY(GT_MSG_START_FLASH, hwnd), param);
}

void refresh_fea_type(SmtMap* map, int& fea_type) {
	if (!map) {
		return;
	}
	fea_type = sdb::datasource::feature_type_of(map->GetActiveOgrLayer());
}

}  // namespace

namespace tool
{
	SmtSelectTool::SmtSelectTool()
	{
		set_name(CST_STR_SELECT_TOOL_NAME.c_str());
	}

	SmtSelectTool::~SmtSelectTool()
	{
		SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

		this->EndDelegate();

		SmtDataSourceMgr::DestoryMemVecLayer(m_resultLayer);

		UnRegisterMsg();
	}

	int SmtSelectTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtLogManager * pLogMgr = SmtLogManager::get_singleton_ptr();
		SmtLog *pLog = pLogMgr->get_log(C_STR_SELECT_TOO_LOG);
		if (NULL == pLog)
		{
			pLogMgr->create_log(C_STR_SELECT_TOO_LOG.c_str());
		}

		m_resultLayer = SmtDataSourceMgr::CreateMemVecLayer();

		SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

		SmtSysPra sysPra = pSysMgr->get_sys_pra();

		m_dpMargin = sysPra.fSmargin;

		append_func_items("点选",GT_MSG_SELECT_POINTSEL,FIM_2DVIEW|FIM_2DMFMENU);
		append_func_items("框选",GT_MSG_SELECT_RECTSEL,FIM_2DVIEW|FIM_2DMFMENU);
		append_func_items("多边形选取",GT_MSG_SELECT_POLYGONSEL,FIM_2DVIEW|FIM_2DMFMENU);
		append_func_items("清除选择要素",GT_MSG_SELECT_CLEAR,FIM_2DVIEW|FIM_2DMFMENU);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POINTSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_RECTSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POLYGONSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_CLEAR);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SEL_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SEL_MODE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtSelectTool::AuxDraw()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtSelectTool::Timer()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtSelectTool::notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{

			}
			break;
		case GT_MSG_SELECT_POINTSEL:
			{
				m_selMode = ST_Point;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_RECTSEL:
			{
				m_selMode = ST_Rect;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_POLYGONSEL:
			{
				m_selMode = ST_Polygon;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_CLEAR:
			{
				clear_scratch(m_resultLayer);
				post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

				SetActive();
			}
			break;

		case GT_MSG_SET_SEL_MODE:
			{
				m_selMode = eSelectMode(*(ushort*)param.wParam);

				OnSetSelMode();

				param.bModify = true;
			}
			break;
		case GT_MSG_GET_SEL_MODE:
			{
				*(ushort*)param.wParam = m_selMode;
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	void SmtSelectTool::OnRetDelegate(int nRetType)
	{
		switch (nRetType)
		{
		case GT_MSG_RET_INPUT_POINT:
			{
				if (!(GetAsyncKeyState( VK_LCONTROL ) & 0x8000))
					clear_scratch(m_resultLayer);

				m_gQDes.fSmargin = m_dpMargin/m_pRenderDevice->GetBlc();
				if (m_resultLayer.layer)
				{
					m_pOperMap->QueryFeature(&m_gQDes,&m_pQDes,m_resultLayer.layer,m_nLayerFeaType);
				}
				refresh_fea_type(m_pOperMap, m_nLayerFeaType);

				SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

				post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);
				post_flash_start(m_hWnd);
			}
			break;
		case GT_MSG_RET_INPUT_LINE:
			{
				if (!(GetAsyncKeyState( VK_LCONTROL ) & 0x8000))
					clear_scratch(m_resultLayer);

				m_gQDes.fSmargin = m_dpMargin/m_pRenderDevice->GetBlc();
				if (m_resultLayer.layer)
				{
					m_pOperMap->QueryFeature(&m_gQDes,&m_pQDes,m_resultLayer.layer,m_nLayerFeaType);
				}
				refresh_fea_type(m_pOperMap, m_nLayerFeaType);

				SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

				post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);
				post_flash_start(m_hWnd);

				const int count = ogr_feature_count(m_resultLayer.layer);
				if (count > 1)
				{
					uint unID = SMT_C_INVALID_UINT_VALUE;
					std::vector<uint> vIDs;
					collect_fids(m_resultLayer.layer, vIDs);
					SmtSelectOneDlg(unID, vIDs);
				}
			}
			break;
		}
	}

	void SmtSelectTool::OnSetSelMode(void)
	{
		this->EndDelegate();
	}

	void SmtSelectTool::apply_draft(const tool::Draft& draft)
	{
		if (!m_pRenderDevice || draft.points.empty())
		{
			return;
		}

		SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

		auto to_lp = [this](const tool::DraftPoint& p, float& x, float& y) {
			m_pRenderDevice->DPToLP(p.x_px, p.y_px, x, y);
		};

		if (draft.kind == tool::DraftKind::kPoint)
		{
			float x = 0;
			float y = 0;
			to_lp(draft.points[0], x, y);
			m_gQDes.pQueryGeom = new OGRPoint(x, y);
			OnRetDelegate(GT_MSG_RET_INPUT_POINT);
			return;
		}

		OGRLinearRing* ring = new OGRLinearRing();
		for (const tool::DraftPoint& p : draft.points)
		{
			float x = 0;
			float y = 0;
			to_lp(p, x, y);
			ring->addPoint(x, y);
		}
		ring->closeRings();
		m_gQDes.pQueryGeom = ring;
		OnRetDelegate(GT_MSG_RET_INPUT_LINE);
	}

	int SmtSelectTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		if (!(GetKeyState( VK_CONTROL ) & 0x8000))
		{
			switch (nChar)
			{
			case 'c':
			case 'C':
				{
					clear_scratch(m_resultLayer);
					post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

					SetActive();
				}
				break;
			}
		}

		return SmtBaseTool::KeyDown(nChar,nRepCnt,nFlags);
	}
}
