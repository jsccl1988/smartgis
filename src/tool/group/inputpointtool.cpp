#include <cstdint>

#include "base/core/api.h"
#include "tool/group/inputpointtool.h"
#include "base/style/stylemanager.h"
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "algorithm/geo/geometry.h"
#include "tool/group/defs.h"
#include "sys/sysmanager.h"
#include "base/style/style_api.h"

#include "tool/group/resource.h"

using namespace render;
using namespace base;
using namespace sdb;
using namespace geo;
using namespace sys;

const string						CST_STR_INPUTPOINT_TOOL_NAME	= "???";

namespace tool
{
	SmtInputPointTool::SmtInputPointTool():m_pGeom(NULL)
		,m_appendType(PT_DOT)
		,m_fAngle(0.)
	{
		 set_name(CST_STR_INPUTPOINT_TOOL_NAME.c_str());
	}

	SmtInputPointTool::~SmtInputPointTool()
	{
		SMT_SAFE_DELETE(m_pGeom);

		UnRegisterMsg();
	}

	int SmtInputPointTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtStyleManager * pStyleMgr = SmtStyleManager::get_singleton_ptr();
		SmtStyle *pStyle = pStyleMgr->get_style(m_szStyleName);
		pStyle->set_style_type(ST_PenDesc|ST_BrushDesc|ST_SymbolDesc|ST_AnnoDesc);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_INPUT_POINT_TYPE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_INPUT_POINT_TYPE);

		RegisterMsg();
		OnSetPointType();

		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::AuxDraw()
	{
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		switch (nMsg)
		{
		case GT_MSG_SET_INPUT_POINT_TYPE:
			{
				m_appendType = *(ushort*)param.wParam;
				OnSetPointType();
			}
			break;
		case GT_MSG_GET_INPUT_ANNO_ANGLE:
			{
				*(float*)param.wParam = m_fAngle;
			}
			break;
		case GT_MSG_GET_INPUT_POINT_TYPE:
			{
				*(ushort*)param.wParam = m_appendType;
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	void SmtInputPointTool::OnSetPointType(void)
	{
		SetActive();
	}

	void SmtInputPointTool::apply_draft(const tool::Draft& draft)
	{
		if (!m_pRenderDevice || draft.points.empty())
		{
			return;
		}
		float x = 0;
		float y = 0;
		m_pRenderDevice->DPToLP(draft.points[0].x_px, draft.points[0].y_px, x, y);
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = new OGRPoint(x, y);
		m_fAngle = 0;
		EndAppendPoint();
	}

	void SmtInputPointTool::EndAppendPoint()
	{
		if (m_pRenderDevice)
		{
			m_pRenderDevice->Refresh();
		}

		ushort		uRetType = GT_MSG_RET_INPUT_POINT;
		SmtListenerMsg param;

		param.hSrcWnd = m_hWnd;
		param.wParam = WPARAM(m_pGeom);
		param.lParam = LPARAM(&uRetType);

		EndIA(GT_MSG_RET_DELEGATE,param);

		SMT_SAFE_DELETE(m_pGeom);

		SetOperDone(true);
	}
}
