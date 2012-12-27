#include "gt_base3dtool.h"

namespace Smt_GroupTool
{
	SmtBase3DTool::SmtBase3DTool():m_p3DRenderDevice(NULL)
		,m_pScene(NULL)
	{
		;
	}

	SmtBase3DTool::~SmtBase3DTool()
	{
		m_p3DRenderDevice = NULL;
		m_pScene = NULL;
	}

	int SmtBase3DTool::Init(LP3DRENDERDEVICE p3DRenderDevice,SmtScene *pScene,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtIATool::Init(hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		if (NULL == p3DRenderDevice ||
			NULL == pScene)
		{
			return SMT_ERR_FAILURE;
		}

		m_p3DRenderDevice = p3DRenderDevice;
		m_pScene		= pScene;

		return SMT_ERR_NONE;
	}
}