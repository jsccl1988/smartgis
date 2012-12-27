/*
File:    gt_traceballtool.h

Desc:    Smt3DViewCtrlTool,3D模型浏览工具

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_VIEW3DTOOL_H
#define _GT_VIEW3DTOOL_H
#include "gt_base3dtool.h"
#include "rd3d_camera.h"
#include "bl3d_object.h"

using namespace Smt_3DBase;

namespace Smt_GroupTool
{
	enum eCursorType3D
	{
		CursorTraceBall,
		CursorSphereCamare,
		CursorFirstPerson,
	};

	class Smt3DViewCtrlTool:public SmtBase3DTool
	{
	public:
		Smt3DViewCtrlTool();
		virtual ~Smt3DViewCtrlTool();

	public:
		int						Init(LP3DRENDERDEVICE p3DRenderDevice,SmtScene *pScene,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int						AuxDraw();
		int						Timer();

	public:
		int						Notify(long nMsg,SmtListenerMsg &param);

		int						SetCursor(void);
		int						LButtonDown(uint nFlags, lPoint point);
		int						MouseMove(uint nFlags, lPoint point);
		int						LButtonUp(uint nFlags, lPoint point);
		int						MouseWeel(uint nFlags, short zDelta,lPoint point);
		int						KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	protected:
		void					TrackballProv(lPoint pos, Vector3& vec);
		
	protected:
		eView3DMode				m_viewMode;
		SmtPerspCamera			*m_pCamera;
		Vector3					m_vOrgTarget,m_vOrgEye,m_vOrgUp;

		lPoint					m_pntOrigin;
		lPoint					m_pntPre;
		lPoint					m_pntCur;
		bool					m_bIsDrag;

		int						m_nWinWidth;
		int						m_nWinHeight;
		float					m_fAngle;
		Vector3					m_vAxis;
		Vector3					m_vPrePos;

		HCURSOR					m_hCursors[3];
	
		vSmt3DObjectPtrs		m_vSelTargetPtrs;
		Smt3DObject				*m_pLastTarget;
		SmtMaterial				m_matLastTarget;
		SmtMaterial				m_matSel;
	};
}

#endif //_GT_VIEW3DTOOL_H