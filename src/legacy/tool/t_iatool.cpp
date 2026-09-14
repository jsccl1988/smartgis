#include "legacy/tool/t_iatool.h"
#include <algorithm>

#include "legacy/tool/t_iatoolmanager.h"

namespace tool
{
	SmtIATool::SmtIATool():m_hWnd(NULL)
		,m_nOper(-1)
		,m_bDone(false)
		,m_bEnableContexMenu(true)
		,m_pDelegateSrc(NULL)
		,m_pDelegateTag(NULL)
	{
		Register();
	}

	SmtIATool::~SmtIATool()
	{
		UnRegister();
	}

	int SmtIATool::Register()
	{
		SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
		return pIAToolMgr->RegisterIATool(this);
	}

	int SmtIATool::RegisterMsg()
	{
		SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
		pIAToolMgr->RegisterIAToolMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtIATool::UnRegister()
	{
		SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
		return pIAToolMgr->RemoveIATool(this);
	}

	int SmtIATool::UnRegisterMsg()
	{
		SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
		pIAToolMgr->UnRegisterIAToolMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtIATool::SetActive()
	{
		SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();
		pIAToolMgr->SetActiveIATool(this);

		return SMT_ERR_NONE;
	}
	//////////////////////////////////////////////////////////////////////////
	int SmtIATool::AuxDraw()
	{
		if(m_pDelegateTag != NULL && !m_pDelegateTag->IsOperDone())
			m_pDelegateTag->AuxDraw();

		return SMT_ERR_NONE;
	}
	int SmtIATool::Timer()
	{
		if(m_pDelegateTag != NULL && !m_pDelegateTag->IsOperDone())
			m_pDelegateTag->Timer();

		return SMT_ERR_NONE;
	}

	bool SmtIATool::SetDelegateSrc(SmtIATool *pDeleSrcTool)
	{
		if(m_pDelegateSrc)
			return false;

		m_pDelegateSrc = pDeleSrcTool;

		return true;
	}

	//��ʼί�� 
	bool SmtIATool::BeginDelegate (SmtIATool * pDelegateTag)
	{
		if(m_pDelegateTag)
			return false;

		m_pDelegateTag = pDelegateTag;
		m_pDelegateTag->SetDelegateSrc(this);

		return true;
	}

	int SmtIATool::EndDelegate (bool bReleaseTarTool)
	{
		if (bReleaseTarTool)
		{
			SMT_SAFE_DELETE(m_pDelegateTag);
		}
		else
			m_pDelegateTag = NULL;

		return SMT_ERR_NONE;
	}

	int SmtIATool::SetCursor(void) 
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->SetCursor();
		else
			::SetCursor(m_hCrossCursor);

		return SMT_ERR_NONE;
	}

	int SmtIATool::LButtonDown(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->LButtonDown(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::LButtonUp(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->LButtonUp(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::LButtonDClick(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->LButtonDClick(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::RButtonDown(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->RButtonDown(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::RButtonUp(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->RButtonUp(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::RButtonDClick(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->RButtonDClick(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::MouseMove(uint nFlags, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->MouseMove(nFlags, point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::MouseWeel(uint nFlags, short zDelta, lPoint point)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->MouseWeel(nFlags,zDelta,point);

		return SMT_ERR_NONE;
	}

	int SmtIATool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->KeyDown(nChar,nRepCnt,nFlags);

		return SMT_ERR_NONE;
	}

	inline bool SmtIATool::IsOperDone(void) 
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->IsOperDone();

		return m_bDone;
	}

	inline void SmtIATool::SetOperDone(bool bDone) 
	{
		if(m_pDelegateTag != NULL)
			m_pDelegateTag->SetOperDone(bDone);

		m_bDone = bDone;
	}

	inline bool SmtIATool::IsEnableContexMenu(void) 
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->IsEnableContexMenu();

		return m_bEnableContexMenu;
	}

	inline void SmtIATool::SetEnableContexMenu(bool bEnable) 
	{
		if(m_pDelegateTag != NULL)
			m_pDelegateTag->SetEnableContexMenu(bEnable);

		m_bEnableContexMenu = bEnable;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtIATool::Init(HWND hWnd,pfnToolCallBack pfnCallBack,void*	pToFollow)
	{
		if (NULL == hWnd)
			return SMT_ERR_INVALID_PARAM;
		 
		m_pfnCallBack	= pfnCallBack ;
		m_pToFollow		= pToFollow;
		m_hWnd			= hWnd;
		m_hCrossCursor	= ::LoadCursor(NULL,IDC_CROSS);

		return SMT_ERR_NONE;
	}

	int SmtIATool::EndIA(long nMsg,SmtListenerMsg &param)
	{
		param.pToFollow = m_pToFollow;
		if (NULL != m_pfnCallBack)
			(*m_pfnCallBack)(nMsg,param);

		if(NULL != m_pDelegateSrc)
			m_pDelegateSrc->notify(nMsg,param);

		return SMT_ERR_NONE;
	}
}