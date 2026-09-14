/*
File:    t_iatool.h

Desc:    SmartGis listener

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _T_IATOOL_H
#define _T_IATOOL_H

#include "base/core/bas_struct.h"
#include "base/core/listener.h"
#include "base/core/msg_def.h"
#include "legacy/tool/tool_export.h"

using namespace base;

namespace tool
{
	typedef int	(*pfnToolCallBack)(long nMsg,SmtListenerMsg &param);

	class TOOL_EXPORT SmtIATool:public SmtListener
	{
	public:
		SmtIATool();
		virtual ~SmtIATool(void);

	public:
		virtual int						Register(void);
		virtual int						RegisterMsg(void);

		virtual int						UnRegister(void);
		virtual int						UnRegisterMsg(void);

		virtual int						SetActive();

	public:
		virtual int						Init(HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void*	pToFollow = NULL);
		virtual int						AuxDraw(void);
		virtual int						Timer(void);
		
		virtual int						EndIA(long nMsg,SmtListenerMsg &param);

	public:
		HWND							GetOwnerWnd() {return m_hWnd;}

	public:
	public:
		virtual bool					BeginDelegate(SmtIATool * pDelegateTag);
		virtual int						EndDelegate(bool bReleaseTarTool = true);

	public:
		virtual int						SetCursor(void);

		virtual int						LButtonDown(uint nFlags,lPoint point) ;
		virtual int						LButtonUp(uint nFlags, lPoint point);
		virtual int						LButtonDClick(uint nFlags, lPoint point);

		virtual int						RButtonDown(uint nFlags, lPoint point);
		virtual int						RButtonUp(uint nFlags, lPoint point);
		virtual int						RButtonDClick(uint nFlags, lPoint point);
		
		virtual int						MouseMove(uint nFlags,lPoint point);
		virtual int						MouseWeel(uint nFlags, short zDelta, lPoint point) ;

		virtual int						KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	public:
		inline bool						IsOperDone(void);
		inline void						SetOperDone(bool bDone);
		inline bool						IsEnableContexMenu(void);
		inline void						SetEnableContexMenu(bool bEnable);

	protected:
		virtual bool					SetDelegateSrc(SmtIATool *pDeleSrcTool);

	protected:
		int								m_nOper;
		bool							m_bDone;
		bool							m_bEnableContexMenu;

	protected:
		HWND							m_hWnd;
		HCURSOR							m_hCrossCursor;

	protected:
		pfnToolCallBack					m_pfnCallBack;
		void*							m_pToFollow;

		SmtIATool						*m_pDelegateSrc;
		SmtIATool						*m_pDelegateTag;
	};

	typedef vector<SmtIATool*>			vSmtIAToolPtrs;
}

#endif //_T_IATOOL_H