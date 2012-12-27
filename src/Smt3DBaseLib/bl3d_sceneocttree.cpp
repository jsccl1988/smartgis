#include "bl3d_sceneocttree.h"

namespace Smt_3DBase
{
	int						g_nSceneMaxTargets = 40;
	int						g_nSceneMaxSubdivision = 6;
	int						g_nSceneCurrentSubdivision = 0;
	int						g_nSceneCurRenderTarget = 0;
	int						g_nSceneTotalLeafNode = 0;

	//////////////////////////////////////////////////////////////////////////
	//OctTree
	//////////////////////////////////////////////////////////////////////////
	SmtSceneOctTree::SmtSceneOctTree():m_pRootNode(NULL)
		,m_nAllRenderTargetsNum(0)
		,m_bShowNodeBox(true)
	{

	}

	SmtSceneOctTree::~SmtSceneOctTree()
	{
		DestroyTree();
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSceneOctTree::CreateOctTree(vSmt3DObjectPtrs &v3DObjectPtrs)
	{
		if (v3DObjectPtrs.size() < 1)
			return SMT_ERR_INVALID_PARAM;

		DestroyTree();

		m_nAllRenderTargetsNum = v3DObjectPtrs.size();

		m_pRootNode = new SmtSceneOctTreeNode();

		//获取渲染目标列表oobb
		GetSceneDimensions(v3DObjectPtrs);

		m_pRootNode->fWidth  = max(m_aabbScene.vcMax.x-m_aabbScene.vcMin.x,m_aabbScene.vcMax.y-m_aabbScene.vcMin.y); 
		m_pRootNode->fWidth  = max(m_aabbScene.vcMax.z-m_aabbScene.vcMin.z,m_pRootNode->fWidth);
		
		m_aabbScene.Merge(m_aabbScene.vcCenter-m_pRootNode->fWidth/2);
		m_aabbScene.Merge(m_aabbScene.vcCenter+m_pRootNode->fWidth/2);

		m_pRootNode->vCenterPos = m_aabbScene.vcCenter;

		m_pRootNode->CreateNode(v3DObjectPtrs,v3DObjectPtrs.size(),m_pRootNode->vCenterPos,m_pRootNode->fWidth);
		
		return SMT_ERR_NONE;
	}

	long SmtSceneOctTree::DestroyTree()
	{
		SMT_SAFE_DELETE(m_pRootNode);
	
		g_nSceneCurrentSubdivision = 0;
		m_nAllRenderTargetsNum = 0;

		m_aabbScene  = Aabb();
		
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtSceneOctTree::GetSceneDimensions(vSmt3DObjectPtrs &v3DObjectPtrs)
	{
		vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
		while(iter != v3DObjectPtrs.end())
		{
			m_aabbScene.Merge((*iter)->GetAabb());
			iter ++;
		}
		
		m_aabbScene.vcCenter = (m_aabbScene.vcMax+m_aabbScene.vcMin)/2.;

	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSceneOctTree::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		if (NULL != m_pRootNode)
		{
			m_pRootNode->UpdateNodeObject(p3DRenderDevice,fElapsed);
		}
		return SMT_ERR_NONE;
	}

	long SmtSceneOctTree::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL != m_pRootNode)
		{
			g_nSceneCurRenderTarget = 0;
			p3DRenderDevice->GetFrustum(m_Frustum);
			m_pRootNode->RenderNodeObject(p3DRenderDevice,m_Frustum,m_bShowNodeBox);
		}
		return SMT_ERR_NONE;
	}

	long SmtSceneOctTree::Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects, LP3DRENDERDEVICE p3DRenderDevice,const lPoint&  point )
	{
		if (NULL != m_pRootNode)
		{
			g_nSceneCurRenderTarget = 0;
			p3DRenderDevice->GetFrustum(m_Frustum);
			m_pRootNode->SelectNodeObject(vSelected3DObjects,p3DRenderDevice,m_Frustum,point);
		}

		return SMT_ERR_FAILURE;	
	}

	void SmtSceneOctTree::ObjectModelMatrixMultiply(Matrix&matTransform)
	{
		if (NULL != m_pRootNode)
		{
			m_pRootNode->NodeObjectModelMatrixMultiply(matTransform);
		}
	}

	void SmtSceneOctTree::ObjectWordlMatrixMultiply(Matrix&matTransform)
	{
		if (NULL != m_pRootNode)
		{
			m_pRootNode->NodeObjectWorldMatrixMultiply(matTransform);
		}
	}

	void  SmtSceneOctTree::GetDebugString(char *szBuf,int nBufLength)
	{
		snprintf( szBuf, nBufLength, "render target:%d/%d;subdivision:%d,-leaf:%d",g_nSceneCurRenderTarget,m_nAllRenderTargetsNum,g_nSceneCurrentSubdivision,g_nSceneTotalLeafNode);
	}

}