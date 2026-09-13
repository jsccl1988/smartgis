/*
File:    3dscene.h

Desc:   

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_SCENE_H
#define _BL3D_SCENE_H

#include "legacy_render/scene3d/bl3d_object.h"
#include "legacy_render/render3d/3drenderdevice.h"
#include "legacy_render/scene3d/bl3d_sceneocttree.h"
#include "base/core/core.h"
#include "base/core/timer.h"
#include "legacy_render/model3d/northarray.h"
#include "legacy_render/render3d/base.h"
#include "legacy_render/render3d/camera.h"
#include "base/core/cslock.h"

using namespace base;
using namespace render;
using namespace render;
using namespace render;

namespace render
{
	class SCENE3D_EXPORT_CLASS SmtScene
	{
	public:
		SmtScene(void);
		virtual ~SmtScene(void);

	public:
		inline		LP3DRENDERDEVICE	Get3DRenderDevice() {return m_p3DRenderDevice;}
		inline		void				Set3DRenderDevice(LP3DRENDERDEVICE p3DRenderDevice) {m_p3DRenderDevice = p3DRenderDevice;}

		inline		SmtPerspCamera*		GetSceneCamera() {return m_pCamera;}
		inline		void				SetSceneCamera(SmtPerspCamera *pCamera) ;

		inline		Aabb&				GetAabb() {return m_aAbb;}
		inline		void				SetAabb(Aabb& aabb) {m_aAbb = aabb;}

	public:
		long							Setup(void);
		long							Update(void); 
		long							Render(void);

	public:
		long							Transform2DTo3D(Vector3 &vOrg,Vector3 &vTar,const lPoint &point);
		long							Transform3DTo2D(const Vector3 &ver3D,lPoint &point);

	public:
		void							Add3DObject(Smt3DObject* p3DObject);
		Smt3DObject*					Get3DObject(int index);
		const Smt3DObject*				Get3DObject(int index) const;
		void							Remove3DObject(Smt3DObject* p3DObject);
		void							Remove3DObject(int index);
		void							Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs);

		void							CreateOctTreeSceneMgr(void);

	public:
		void							SetShowNodeBox(bool bShowNodeBox = true ) {m_bShowNodeBox = bShowNodeBox;}
		bool							IsShowNodeBox(void)	{return m_bShowNodeBox;}

	public:
		//�任��ģ����������
		long							TransModel3DObjects(Matrix&matTransform);

		//�任��ģ���������
		long							TransWorld3DObjects(Matrix&matTransform);

		//ʰȡ
		long							Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects, lPoint point );

	protected:
		bool							Update3DObjCatalog(void);

	private:
		LP3DRENDERDEVICE				m_p3DRenderDevice;

		SmtSceneOctTree					*m_pSceneTree;
		bool							m_bOctTreeCreated;
		bool							m_bShowNodeBox;
		vSmt3DObjectPtrs				m_v3DObjectPtrs;

		SmtTimer						*m_pTimer;
		SmtPerspCamera					*m_pCamera;
		SmtNorthArray					*m_pNorthArray;

		Vector3							m_vOrgPos;					//��������ϵԭ��
		Aabb							m_aAbb;						//aabb��Χ��

		uint							m_nHelpInfoFont;
		uint							m_nRenderInfoFont;
		uint							m_nTimerInfoFont;

		char							m_szHelpInfoBuf[TEMP_BUFFER_SIZE];
		char							m_szRenderInfoBuf[TEMP_BUFFER_SIZE];

	};
}
#if     !defined(SCENE3D_EXPORTS)
#if     defined(_DEBUG)
#          pragma comment(lib,"legacy_render_d.lib")
#       else
#          pragma comment(lib,"legacy_render.lib")
#	    endif
#endif

#endif //_BL3D_SCENE_H