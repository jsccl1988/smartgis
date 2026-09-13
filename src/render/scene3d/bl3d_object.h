/*
File:    bl3d_object.h

Desc:    ��ά������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_RENDERABLE_H
#define _BL3D_RENDERABLE_H
#include "render/render3d/base.h"
#include "render/render3d/3drenderdevice.h"
using namespace render;

namespace render
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
		bool						m_bVisible;				//�Ƿ�ɼ�
		double						m_dbfTransparent;		//͸����
		Aabb						m_aAbb;					//aabb��Χ��
		SmtMaterial					m_matMaterial;			//ģ�Ͳ���
		string						m_strTexName;			//��������
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
		Vector3						m_vOrgPos;				//ģ������ϵԭ��
		Matrix						m_mtxModel;				//ģ����������ϵ�任����
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
		Matrix						m_mtxWorld;			//��������ϵ�任����

	};

	typedef vector<Smt3DObject*>	vSmt3DObjectPtrs;
}

#endif //_BL3D_RENDERABLE_H