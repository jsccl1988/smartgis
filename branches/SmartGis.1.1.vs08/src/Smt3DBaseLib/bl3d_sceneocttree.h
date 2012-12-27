/*
File:    bl3d_sceneoctree.h 

Desc:    八叉树

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_SCENEOCTREE_H
#define _BL3D_SCENEOCTREE_H

#include "smt_core.h"

#include "ml3d_mathlib.h"
#include "rd3d_base.h"
#include "bl3d_object.h"
#include "bl3d_bas_struct.h "

namespace Smt_3DBase
{
	extern int							g_nSceneMaxTargets;
	extern int							g_nSceneMaxSubdivision;
	extern int							g_nSceneCurrentSubdivision;
	extern int							g_nSceneCurRenderTarget;
	extern int							g_nSceneTotalLeafNode;

	class SmtSceneOctTree;
	class SMT_EXPORT_CLASS SmtSceneOctTreeNode
	{
		friend class SmtSceneOctTree;
	public:
		SmtSceneOctTreeNode();
		~SmtSceneOctTreeNode();
	public:
		//创建节点
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
		//是否在节点包围盒内
		bool							IsInOctNodeAabbBox(const Vector3& point);

		//寻找点所在的最小包围盒所在节点指针
		SmtSceneOctTreeNode*			FindMinBoxOctNode(const Ray &ray );

		//寻找点所在的最小包围盒所在节点指针
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
	class SMT_EXPORT_CLASS SmtSceneOctTree:public Smt3DRenderable,public Smt3DMovable
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

#if     !defined(Export_Smt3DBaseLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DBaseLibD.lib")
#       else
#          pragma comment(lib,"Smt3DBaseLib.lib")
#	    endif
#endif


#endif //_BL3D_SCENEOCTREE_H