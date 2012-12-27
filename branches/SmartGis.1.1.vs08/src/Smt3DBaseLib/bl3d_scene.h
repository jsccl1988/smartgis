/*
File:    3dscene.h

Desc:   

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_SCENE_H
#define _BL3D_SCENE_H

#include "bl3d_object.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_sceneocttree.h"
#include "smt_core.h"
#include "smt_timer.h"
#include "md3d_northarray.h"
#include "rd3d_base.h"
#include "rd3d_camera.h"
#include "smt_cslock.h"

using namespace Smt_Core;
using namespace Smt_3Drd;
using namespace Smt_3DMath;
using namespace Smt_3DModel;

namespace Smt_3DBase
{
	class SMT_EXPORT_CLASS SmtScene
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
		//变换，模型自身矩阵
		long							TransModel3DObjects(Matrix&matTransform);

		//变换，模型世界矩阵
		long							TransWorld3DObjects(Matrix&matTransform);

		//拾取
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

		Vector3							m_vOrgPos;					//场景坐标系原点
		Aabb							m_aAbb;						//aabb包围盒

		uint							m_nHelpInfoFont;
		uint							m_nRenderInfoFont;
		uint							m_nTimerInfoFont;

		char							m_szHelpInfoBuf[TEMP_BUFFER_SIZE];
		char							m_szRenderInfoBuf[TEMP_BUFFER_SIZE];

	};
}
#if     !defined(Export_Smt3DBaseLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DBaseLibD.lib")
#       else
#          pragma comment(lib,"Smt3DBaseLib.lib")
#	    endif
#endif

#endif //_BL3D_SCENE_H