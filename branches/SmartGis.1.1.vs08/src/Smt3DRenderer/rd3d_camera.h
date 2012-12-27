/*
File:    rd3d_camera.h

Desc:    相机

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_CAMERA_H
#define _RD3D_CAMERA_H

#include "rd3d_base.h"

namespace Smt_3Drd
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	//相机基类
	class SMT_EXPORT_CLASS SmtCamera
	{
	public:
		SmtCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport);
		virtual ~SmtCamera(void);

	public:
		void					SetViewport(Viewport3D &viewport) {m_viewport = viewport;}
		 
	public:
		virtual	long			Apply(void);	

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		Viewport3D				m_viewport;
	};

	//正射相机
	class  SMT_EXPORT_CLASS SmtOrthCamera:public SmtCamera
	{
	public:
		SmtOrthCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport);
		virtual ~SmtOrthCamera(void);

	public:
		inline void				SetIdentity(bool value) {m_bIdentity = value;}
		inline bool				GetIdentity() {return m_bIdentity;}
		inline void				SetInverse(bool value) {m_bInverse = value;}
		inline bool				GetInverse() {return m_bInverse;}

	public:
		virtual	long			Apply(void);	

	protected:
		bool					m_bIdentity;
		bool					m_bInverse;
	};

	//透视相机
	class  SMT_EXPORT_CLASS SmtPerspCamera:public SmtCamera
	{
	public:
		SmtPerspCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport);
		virtual ~SmtPerspCamera(void);

	public:
		virtual	long			Apply(void);

	public:
		inline void				SetEye(Vector3& eye){ m_vEye = eye;}
		inline Vector3&         GetEye(){ return m_vEye;}

		inline void				SetUp(Vector3& up){ m_vUp = up;}
		inline Vector3&         GetUp(){ return m_vUp;}

		inline void				SetTarget(Vector3& target){ m_vTarget = target;}
		inline Vector3&         GetTarget() { return m_vTarget;}

		inline void				SetETU(Vector3 &eye,Vector3 &target,Vector3 &up)
		{
			m_vEye = eye;
			m_vUp = up;
			m_vTarget = target;
		}

		inline	void			SetMoveStep(float fStep) {m_fMoveStep = fStep;}
		inline	float			GetMoveStep(void) {return m_fMoveStep;}

	public:
		//移动眼睛
		void					MoveEyeSmoothly(bool bForward = true);
		void					MoveEyeImmediately(float fDis);

		//改变相机方向
		void                    Pitch(float angle);				//绕x轴
		void                    Yaw(float angle);				//绕y轴
		void                    Roll(float angle);				//绕z轴

		//以固定视距
		void                    MoveForward(void);
		void                    MoveBack(void);
		void                    MoveLeft(void);
		void                    MoveRight(void);
		void                    MoveUp(void);
		void                    MoveDown(void);
		
	protected:
		Vector3                 m_vEye;
		Vector3                 m_vUp;
		Vector3                 m_vTarget;

		float					m_fMoveStep;
		float					m_fSmoothX;
	};

	//第一视角相机
	class  SMT_EXPORT_CLASS SmtFPSCamera:public SmtPerspCamera
	{
	public:
		SmtFPSCamera(LP3DRENDERDEVICE p3DRenderDevice,Viewport3D &viewport);
		virtual ~SmtFPSCamera(void);

	public:
		void					SetWinCenter(lPoint center) {m_winCenter = center;}
		void					SetViewByMouse(void);

	private:
		lPoint					m_winCenter;
	};

	//球面相机
	class  SMT_EXPORT_CLASS SmtArbvCamera:public SmtPerspCamera
	{
	public:
		SmtArbvCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport);
		virtual ~SmtArbvCamera(void);

	public:

		inline	void			SetArbitRaduis(float fRaduis) {m_fRaduis = fRaduis;}
		inline	float			GetArbitRaduis(void) {return m_fRaduis;}

	public:
		void					SetArbitMove(long deltX,long deltY);

	private:
		float					m_fRaduis;
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3DCAMERA_H