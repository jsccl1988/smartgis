/*
File:    bl3d_mdloctree.h 

Desc:    八叉树

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL3D_MDLOCTREE_H
#define _BL3D_MDLOCTREE_H

#include "smt_core.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "ml3d_mathlib.h"
#include "rd3d_base.h"
#include "bl3d_bas_struct.h "
#include "smt_bas_struct.h"

using namespace Smt_3Drd;

namespace Smt_3DBase
{
	extern int						g_nMdlMaxTargets;
	extern int						g_nMdlMaxSubdivision;
	extern int						g_nMdlCurrentSubdivision;
	extern int						g_nMdlCurRenderTarget;
	extern int						g_nMdlTotalLeafNode;

	class SmtVertexOctTree;
	class SMT_EXPORT_CLASS SmtVertexOctTreeNode
	{
		friend class SmtVertexOctTree;
	public:
		SmtVertexOctTreeNode();
		~SmtVertexOctTreeNode();

	public:
		//创建节点
		long						CreateNode(SmtVertex3DList &lstVers,Vector3 vCenter,byte octCode,double width,LP3DRENDERDEVICE p3DRenderDevice);

		//获取子节点中心坐标
		Vector3						GetSubNodeCenter(int nSubID);	
	
		//获取八叉树编码
		uint						GetSubNodeCode(int nSubID);	
		
		//创建子节点
		void						CreateSubNode(SmtVertexOctTreeNode*pParentNode,SmtVertexOctTreeNode*&pSub,SmtVertex3DList &lstVers,vector<bool> vbInSubNode,int nVertexs,int nSubID,LP3DRENDERDEVICE p3DRenderDevice);

		//////////////////////////////////////////////////////////////////////////

		//渲染节点内容
		void						RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,bool bShowOctNodeBox = true);

		//获取子树的深度
		int							GetSubDepth();

		//寻找点所在的最小包围盒所在节点指针
		SmtVertexOctTreeNode*		FindMinBoxOctNode(const Vector3& point);

	protected:
		SmtVertexOctTreeNode		*pParentNode;
		SmtVertexOctTreeNode		*pSubNodes[8];	
		Vector3						vCenterPos;
		double						fWidth;
		uint						unOctCode;
		bool						bSubDivided;
		SmtVertex3DList				vertexList;	
		SmtVertexBuffer*			pVertexBuffer;

		bool						bSelected;
	};

	class SMT_EXPORT_CLASS SmtVertexOctTree
	{
	public:
		SmtVertexOctTree();
		virtual ~SmtVertexOctTree();

	public:
		//创建八叉树
		long						CreateOctTree(SmtVertex3DList &lstVers,LP3DRENDERDEVICE p3DRenderDevice);
		
		//渲染八叉树
		void						RenderTree(LP3DRENDERDEVICE p3DRenderDevice,bool bShowOctNodeBox = true);

		//销毁八叉树
		long						DestroyTree();

		//////////////////////////////////////////////////////////////////////////
		//获取树的深度
		inline int					GetDepth(void){return m_nDepth;}

		//测试
		bool						HitTestOctNode(const Vector3 & point);

		void						GetDebugString(char *szBuf,int nBufLength);

	protected:
		void						GetSceneDimensions(SmtVertex3DList &lstVers);

	protected:
		SmtVertexOctTreeNode		*m_pRootNode;
		Aabb						m_aabbScene;
		SmtFrustum					m_Frustum;
		int							m_nDepth;
	};

}

#if     !defined(Export_Smt3DBaseLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DBaseLibD.lib")
#       else
#          pragma comment(lib,"Smt3DBaseLib.lib")
#	    endif
#endif


#endif //_BL3D_MDLOCTREE_H