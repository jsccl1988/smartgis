/*
File:    bl3d_object.h

Desc:    三维基础库

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_RENDERABLE_H
#define _BL3D_RENDERABLE_H
#include "rd3d_base.h"
#include "rd3d_3drenderdevice.h"
using namespace Smt_3Drd;

namespace Smt_3DBase
{
	class Smt3DRenderable
	{
	public:
		Smt3DRenderable() {m_bVisible = true;}
		virtual ~Smt3DRenderable() {}

	public:
		bool						IsVisible(void) {return m_bVisible;}
		void						SetVisible(bool bVisible = true) {m_bVisible = bVisible;}

		double						GetTransparent(void) {return m_dbfTransparent;}
		void						SetTransparent(double dbfTransparent = 1.) {m_dbfTransparent = dbfTransparent;}

		virtual		long			Render(LP3DRENDERDEVICE p3DRenderDevice) = 0; 
		virtual		bool			Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point) {return false;}

		inline		Aabb&			GetAabb() {return m_aAbb;}
		inline		void			SetAabb(Aabb& aabb) {m_aAbb = aabb;}

		inline		SmtMaterial&	GetMaterial() {return m_matMaterial;}
		inline		void			SetMaterial(SmtMaterial& matMaterial) {m_matMaterial = matMaterial;}

	protected:
		bool						m_bVisible;				//是否可见
		double						m_dbfTransparent;		//透明度
		Aabb						m_aAbb;					//aabb包围盒
		SmtMaterial					m_matMaterial;			//模型材质
		string						m_strTexName;			//纹理名称
	};

	class Smt3DMovable
	{
	public:
		Smt3DMovable() {}
		virtual ~Smt3DMovable() {}

	public:
		inline		Matrix&			GetModelTransMatrix() {return m_mtxModel;}
		inline		void			SetModelTransMatrix(Matrix& vTransform) {m_mtxModel  = vTransform;}
		void						ModelTransMatrixMultiply(Matrix&matTransform) {m_mtxModel = m_mtxModel*matTransform;}

		virtual		long			Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed) = 0; 

	protected:
		Vector3						m_vOrgPos;				//模型坐标系原点
		Matrix						m_mtxModel;				//模型自身坐标系变换矩阵
	};

	class Smt3DObject:public Smt3DRenderable,public Smt3DMovable
	{
	public:
		virtual		long			Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName = "")  
		{
			m_vOrgPos	  = vPos;
			m_matMaterial = matMaterial;
			m_strTexName  = szTexName;
			m_mtxModel.Identity();
			m_mtxWorld.Identity();

			return SMT_ERR_NONE;
		} 
		virtual		long			Create(LP3DRENDERDEVICE p3DRenderDevice) = 0; 
		virtual		long			Render(LP3DRENDERDEVICE p3DRenderDevice) = 0; 
		virtual		long			Destroy() = 0; 

		inline		Matrix&			GetWorldTransMatrix() {return m_mtxWorld;}
		inline		void			SetWorldTransMatrix(Matrix& vTransform) {m_mtxWorld  = vTransform;}
		void						WorldTransMatrixMultiply(Matrix&matTransform) {m_mtxWorld = m_mtxWorld*matTransform;}

	protected:
		Matrix						m_mtxWorld;			//世界坐标系变换矩阵

	};

	typedef vector<Smt3DObject*>	vSmt3DObjectPtrs;
}

#endif //_BL3D_RENDERABLE_H