/*
File:    bl3d_sceneoctree.h 

Desc:    �˲���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_SCENEOCTREE_H
#define _BL3D_SCENEOCTREE_H

#include "base/core/core.h"

#include "render/math/math.h"
#include "legacy/render/render3d/base.h"
#include "legacy/render/scene3d/bl3d_object.h"
#include "legacy/render/scene3d/bl3d_bas_struct.h"

namespace render
{
	extern int							g_nSceneMaxTargets;
	extern int							g_nSceneMaxSubdivision;
	extern int							g_nSceneCurrentSubdivision;
	extern int							g_nSceneCurRenderTarget;
	extern int							g_nSceneTotalLeafNode;

	class SmtSceneOctTree;
	class SCENE3D_EXPORT_CLASS SmtSceneOctTreeNode
	{
		friend class SmtSceneOctTree;
	public:
		SmtSceneOctTreeNode();
		~SmtSceneOctTreeNode();
	public:
		//�����ڵ�
		long							CreateNode(vSmt3DObjectPtrs &v3DObjectPtrs,int nTarget,Vector3 vCenter,float width);

		//
		Vector3							GetSubNodeCenter(int nSubID);		

		//
		void							CreateSubNode(SmtSceneOctTreeNode*pParentNode,SmtSceneOctTreeNode*&pSub,vSmt3DObjectPtrs &v3DObjectPtrs,vector<bool> vbInSubNode,int nTargets,int nSubID);

		void							UpdateNodeObject(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed);
		void							RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,bool bShowOctNodeBox = true);
		void							SelectNodeObject(vSmt3DObjectPtrs &vSelected3DObjects, LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,const lPoint& point );

		void							NodeObjectModelMatrixMultiply(Matrix&matTransform);
		void							NodeObjectWorldMatrixMultiply(Matrix&matTransform);


	public:
		//�Ƿ��ڽڵ��Χ����
		bool							IsInOctNodeAabbBox(const Vector3& point);

		//Ѱ�ҵ����ڵ���С��Χ�����ڽڵ�ָ��
		SmtSceneOctTreeNode*			FindMinBoxOctNode(const Ray &ray );

		//Ѱ�ҵ����ڵ���С��Χ�����ڽڵ�ָ��
		SmtSceneOctTreeNode*			FindMinBoxOctNode(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point);

	protected:
		SmtSceneOctTreeNode				*pParentNode;
		SmtSceneOctTreeNode				*pSubNodes[8];	
		Vector3							vCenterPos;
		float							fWidth;
		string							strCode;
		bool							bSubDivided;
		vSmt3DObjectPtrs				v3DObjectPtrs;	
		int								nTargetCount;		
	};

	class SmtScene;
	class SCENE3D_EXPORT_CLASS SmtSceneOctTree:public Smt3DRenderable,public Smt3DMovable
	{
	public:
		friend class SmtScene;
	
		SmtSceneOctTree();
		virtual ~SmtSceneOctTree();

	public:
		void							SetShowNodeBox(bool bShowNodeBox = true ) {m_bShowNodeBox = bShowNodeBox;}
		bool							IsShowNodeBox(void)	{return m_bShowNodeBox;}

		long							CreateOctTree(vSmt3DObjectPtrs &v3DObjectPtrs);
		long							DestroyTree();

	public:
		long							Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long							Render(LP3DRENDERDEVICE p3DRenderDevice); 

		void							GetDebugString(char *szBuf,int nBufLength);

	public:
		void							ObjectModelMatrixMultiply(Matrix&matTransform);
		void							ObjectWordlMatrixMultiply(Matrix&matTransform);

		long							Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects, LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point );

	protected:
		void							GetSceneDimensions(vSmt3DObjectPtrs &v3DObjectPtrs);

	protected:
		SmtSceneOctTreeNode				*m_pRootNode;
		Aabb							m_aabbScene;
		SmtFrustum						m_Frustum;
		bool							m_bShowNodeBox;
		int								m_nAllRenderTargetsNum;
	};
}

#if     !defined(SCENE3D_EXPORTS)
#if     defined(_DEBUG)
#          pragma comment(lib,"legacy_render_d.lib")
#       else
#          pragma comment(lib,"legacy_render.lib")
#	    endif
#endif


#endif //_BL3D_SCENEOCTREE_H