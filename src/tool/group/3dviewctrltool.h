/*
File:    gt_traceballtool.h

Desc:    Smt3DViewCtrlTool,3Dģ���������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_VIEW3DTOOL_H
#define _GT_VIEW3DTOOL_H

#include "tool/group/base3dtool.h"
#include "render/render3d/camera.h"
#include "render/scene3d/bl3d_object.h"
#include "tool/gestures.h"

using namespace render;

namespace tool
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
		int						notify(long nMsg,SmtListenerMsg &param);

		int						SetCursor(void);
		int						KeyDown(uint nChar, uint nRepCnt, uint nFlags);
		void					apply_draft(const tool::Draft& draft) override;

	protected:
		void					ReplaceCamera(render::View3dCameraKind kind, float step_div);
		void					TrackballProv(lPoint pos, Vector3& vec);
		void					OnSetViewMode(void);
		void					ApplyCameraKey(uint nChar);
		void					ApplyPick(const tool::Draft& draft);
		void					ApplyModeKey(uint nChar);
		void					RestoreLastTarget();

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
