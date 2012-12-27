#include "bl3d_vertexocttree.h"
#include "smt_bas_struct.h"

namespace Smt_3DBase
{
	int						g_nMdlMaxTargets = 100;
	int						g_nMdlMaxSubdivision = 5;
	int						g_nMdlCurrentSubdivision = 0;
	int						g_nMdlCurRenderTarget = 0;
	int						g_nMdlTotalLeafNode = 0;

	//////////////////////////////////////////////////////////////////////////
	//SmtVertexOctTree
	//////////////////////////////////////////////////////////////////////////
	SmtVertexOctTree::SmtVertexOctTree():m_pRootNode(NULL)
		,m_nDepth(0)
	{

	}

	SmtVertexOctTree::~SmtVertexOctTree()
	{
		DestroyTree();
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtVertexOctTree::CreateOctTree(SmtVertex3DList &lstVers,LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (lstVers.nCount < 1)
			return SMT_ERR_INVALID_PARAM;

		DestroyTree();

		m_pRootNode = new SmtVertexOctTreeNode();

		//获取渲染目标列表oobb
		GetSceneDimensions(lstVers);

		m_pRootNode->fWidth  = max(m_aabbScene.vcMax.x-m_aabbScene.vcMin.x,m_aabbScene.vcMax.y-m_aabbScene.vcMin.y); 
		m_pRootNode->fWidth  = max(m_aabbScene.vcMax.z-m_aabbScene.vcMin.z,m_pRootNode->fWidth);
		
		m_aabbScene.Merge(m_aabbScene.vcCenter-m_pRootNode->fWidth/2);
		m_aabbScene.Merge(m_aabbScene.vcCenter+m_pRootNode->fWidth/2);

		m_pRootNode->vCenterPos = m_aabbScene.vcCenter;
		m_pRootNode->unOctCode = 0;

		m_pRootNode->CreateNode(lstVers,m_pRootNode->vCenterPos,m_pRootNode->unOctCode,m_pRootNode->fWidth,p3DRenderDevice);

		m_nDepth = m_pRootNode->GetSubDepth();

		return SMT_ERR_NONE;
	}

	long SmtVertexOctTree::DestroyTree()
	{
		SMT_SAFE_DELETE(m_pRootNode);

		g_nMdlCurrentSubdivision = 0;

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtVertexOctTree::GetSceneDimensions(SmtVertex3DList &lstVers)
	{
		for (int i = 0 ; i < lstVers.nCount; i ++ )
		{
			Vector3 vPos = lstVers.pVertexs[i].ver;
			m_aabbScene.Merge(vPos);
		}

		m_aabbScene.vcCenter = (m_aabbScene.vcMax+m_aabbScene.vcMin)/2.;
	}


	//////////////////////////////////////////////////////////////////////////
	void SmtVertexOctTree::RenderTree(LP3DRENDERDEVICE p3DRenderDevice,bool bShowOctNodeBox )
	{
		static char szBuf[TEMP_BUFFER_SIZE];
		if (NULL != m_pRootNode)
		{
			g_nMdlCurRenderTarget = 0;
			p3DRenderDevice->GetFrustum(m_Frustum);
			m_pRootNode->RenderNodeObject(p3DRenderDevice,m_Frustum,bShowOctNodeBox);
			GetDebugString(szBuf,TEMP_BUFFER_SIZE);
			p3DRenderDevice->DrawText(0,10,100,SmtColor(0.,1.,1.),szBuf);
		}
	}

	bool SmtVertexOctTree::HitTestOctNode(const Vector3 & point) 
	{ 
		if(m_pRootNode)
		{
			SmtVertexOctTreeNode *pNode = m_pRootNode->FindMinBoxOctNode(point);
			if (pNode)
			{
				pNode->bSelected = !pNode->bSelected;
				return true;
			}
			else
				return false;
		}

		return false;
	}

	void  SmtVertexOctTree::GetDebugString(char *szBuf,int nBufLength)
	{
		snprintf( szBuf, nBufLength, " Render Point:%d;SmtVertexOctTree-Depth:%d,-TotalLeafNode:%d",g_nMdlCurRenderTarget,m_nDepth,g_nMdlTotalLeafNode);
	} 

}