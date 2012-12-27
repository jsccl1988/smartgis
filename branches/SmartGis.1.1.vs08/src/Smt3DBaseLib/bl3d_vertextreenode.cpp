#include "bl3d_vertexocttree.h"
#include <algorithm>
#include <functional>

namespace Smt_3DBase
{
	//////////////////////////////////////////////////////////////////////////
	//SmtVertexOctTreeNode
	//////////////////////////////////////////////////////////////////////////
	SmtVertexOctTreeNode::SmtVertexOctTreeNode():pVertexBuffer(NULL)
		,pParentNode(NULL)
		,fWidth(0)
		,unOctCode(0)
		,bSubDivided(false)
		,bSelected(false)
	{
		memset(pSubNodes,NULL,sizeof(SmtVertexOctTreeNode*)*8);
	}

	SmtVertexOctTreeNode::~SmtVertexOctTreeNode()
	{
		vertexList.Release();
		SMT_SAFE_DELETE(pVertexBuffer);
		for (int i = 0; i< 8; i++)
		{
			SMT_SAFE_DELETE(pSubNodes[i]);
		}
	}
	//////////////////////////////////////////////////////////////////////////
	long SmtVertexOctTreeNode::CreateNode(SmtVertex3DList &lstVers,Vector3 vCenter,byte octCode,double width,LP3DRENDERDEVICE p3DRenderDevice)
	{
		this->fWidth = width;
		this->vCenterPos = vCenter;
		this->unOctCode = octCode;
		if( (lstVers.nCount > g_nMdlMaxTargets) && (g_nMdlCurrentSubdivision < g_nMdlMaxSubdivision) )
		{
			this->bSubDivided = true;

			vector<bool> vBDealedTarget(lstVers.nCount);
			for (int j = 0; j < lstVers.nCount;j++)
			{
				vBDealedTarget[j] = false;
			}

			vector< vector<bool> > vvbInsideNode;
			vvbInsideNode.resize(8);
			for (int i = 0 ; i < 8; i++)
			{
				vector<bool> &vBInside =  vvbInsideNode[i];
				vBInside.resize(lstVers.nCount);	
				for (int j = 0; j < lstVers.nCount;j++)
				{
					vBInside[j] = false;
				}
			}


			for (int i = 0; i < 8 ; i++)
			{
				Vector3 vSubCenter =  this->GetSubNodeCenter(i);
				vector<bool> &vBInsideNode =  vvbInsideNode[i];
				Aabb	aabbSubNode(Vector3(vSubCenter.x-fWidth/4,vSubCenter.y-fWidth/4,vSubCenter.z-fWidth/4),
									Vector3(vSubCenter.x+fWidth/4,vSubCenter.y+fWidth/4,vSubCenter.z+fWidth/4));

				for (int i = 0; i < lstVers.nCount;i++)
				{
					if (!vBDealedTarget[i] && aabbSubNode.Contains(lstVers.pVertexs[i].ver))
					{
						vBInsideNode[i] = true;
						vBDealedTarget[i] = true;
					}
				}
			}

			int nTargetNum[8] ;

			for (int i = 0 ; i < 8; i++)
			{
				vector<bool> &vBInside =  vvbInsideNode[i];
				nTargetNum[i] = 0;
				for (int j = 0; j < lstVers.nCount;j++)
				{
					if (vBInside[j])
						nTargetNum[i]++;	 
				}
			}


			//创建子节点
			for (int i = 0; i < 8 ; i++)
			{
				vector<bool> &vBInsideNode =  vvbInsideNode[i];
				this->CreateSubNode(this,pSubNodes[i],lstVers,vBInsideNode,nTargetNum[i],i,p3DRenderDevice);
			}
		}
		else
		{
			this->bSubDivided = false;
			this->vertexList= lstVers;
			// Create VB
			this->pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
				this->vertexList.nCount,
				VF_XYZ|VF_DIFFUSE, 
				false );

			this->pVertexBuffer->Lock();

			for (int i = 0 ; i < this->vertexList.nCount; i ++ )
			{
				Vector3 &vPos = (this->vertexList.pVertexs[i]).ver;
				SmtColor &clr = (this->vertexList.pVertexs[i]).clr;
				this->pVertexBuffer->Vertex( vPos.x,vPos.y,vPos.z);
				this->pVertexBuffer->Diffuse( clr.fRed,clr.fGreen,clr.fBlue,clr.fA);
			}

			this->pVertexBuffer->Unlock();

			g_nMdlTotalLeafNode ++;
		}

		return SMT_ERR_NONE;
	}

	uint SmtVertexOctTreeNode::GetSubNodeCode(int nSubID)
	{
		uint unCode = this->unOctCode;
		unCode = (unCode<<3)+nSubID;

		// Return the new node code
		return unCode;
	}

	Vector3 SmtVertexOctTreeNode::GetSubNodeCenter(int nSubID)
	{
		Vector3 vNodeCenter(0, 0, 0);

		Vector3 vCtr = vCenterPos;

		switch(nSubID)							
		{
		case TOP_LEFT_FRONT:
			vNodeCenter = Vector3(vCtr.x - fWidth/4, vCtr.y + fWidth/4, vCtr.z + fWidth/4);
			break;

		case TOP_LEFT_BACK:
			vNodeCenter = Vector3(vCtr.x - fWidth/4, vCtr.y + fWidth/4, vCtr.z - fWidth/4);
			break;

		case TOP_RIGHT_BACK:
			vNodeCenter = Vector3(vCtr.x + fWidth/4, vCtr.y + fWidth/4, vCtr.z - fWidth/4);
			break;

		case TOP_RIGHT_FRONT:
			vNodeCenter = Vector3(vCtr.x + fWidth/4, vCtr.y + fWidth/4, vCtr.z + fWidth/4);
			break;

		case BOTTOM_LEFT_FRONT:
			vNodeCenter = Vector3(vCtr.x - fWidth/4, vCtr.y - fWidth/4, vCtr.z + fWidth/4);
			break;

		case BOTTOM_LEFT_BACK:
			vNodeCenter = Vector3(vCtr.x - fWidth/4, vCtr.y - fWidth/4, vCtr.z - fWidth/4);
			break;

		case BOTTOM_RIGHT_BACK:
			vNodeCenter = Vector3(vCtr.x + fWidth/4, vCtr.y - fWidth/4, vCtr.z - fWidth/4);
			break;

		case BOTTOM_RIGHT_FRONT:
			vNodeCenter = Vector3(vCtr.x + fWidth/4, vCtr.y - fWidth/4, vCtr.z + fWidth/4);
			break;
		}

		// Return the new node center
		return vNodeCenter;
	}

	void SmtVertexOctTreeNode::CreateSubNode(SmtVertexOctTreeNode*pParent,SmtVertexOctTreeNode*&pSub,SmtVertex3DList &lstVers,vector<bool> vbInSubNode,int nTargets,int nSubID,LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (nTargets > 0)
		{
			pSub = new SmtVertexOctTreeNode();
			pSub->pParentNode = pParent;
		
			SmtVertex3DList verTmpList;
			verTmpList.pVertexs = new SmtVertex3D[nTargets];
			verTmpList.nCount = nTargets;
			int iTmpIndex = 0;
			for (int i = 0; i < lstVers.nCount;i++ )
			{
				if (vbInSubNode[i])
				{
					verTmpList.pVertexs[iTmpIndex++] = lstVers.pVertexs[i];
				}
			}

			g_nMdlCurrentSubdivision++;
			pSub->CreateNode(verTmpList,pParent->GetSubNodeCenter(nSubID),pParent->GetSubNodeCode(nSubID),pParent->fWidth/2.,p3DRenderDevice);
			g_nMdlCurrentSubdivision--;

			verTmpList.Release();
		}
	}

	void SmtVertexOctTreeNode::RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,bool bShowOctNodeBox)
	{
		if (smtFrustum.IsCubeIn(vCenterPos.x,vCenterPos.y,vCenterPos.z,fWidth/2.))
		{
			if (bShowOctNodeBox)
			{
				SmtGPUStateManager *stateManager = p3DRenderDevice->GetStateManager();
				stateManager->SetLight(false);
				stateManager->Set2DTextures(false);

				//渲染本节点box
				p3DRenderDevice->DrawCube3D(vCenterPos,fWidth,SmtColor(0.,1.,0.,1.));

				stateManager->SetLight(true);
				stateManager->Set2DTextures(true);
			}
			
			//渲染子节点
			if (bSubDivided)
			{
				for (int i = 0; i < 8 ; i++)
				{
					if (pSubNodes[i])
						pSubNodes[i]->RenderNodeObject(p3DRenderDevice,smtFrustum,bShowOctNodeBox);
				}
			}
			else
			{
				//渲染本节点对象
				if (NULL != pVertexBuffer)
				{
					p3DRenderDevice->DrawPrimitives(
					PT_POINTLIST,
					pVertexBuffer,
					0,
					pVertexBuffer->GetVertexCount());

					g_nMdlCurRenderTarget += pVertexBuffer->GetVertexCount();
				}
			}
		}
	}

	//获取子树深度，包括本节点
	int SmtVertexOctTreeNode::GetSubDepth()
	{	
		if (bSubDivided)
		{
			int nDepth = 0;
			for (int i = 0; i < 8 ; i++)
			{
				if (pSubNodes[i])
					nDepth = max(pSubNodes[i]->GetSubDepth(),nDepth);
			}
			return (++nDepth);
		}
		else
			return 1;
	}

	//寻找点所在的最小包围盒所在节点指针
	SmtVertexOctTreeNode* SmtVertexOctTreeNode::FindMinBoxOctNode(const Vector3& point)
	{
		SmtVertexOctTreeNode *pCurNode = this;

		if(this->bSubDivided)
		{
			while (pCurNode && pCurNode->bSubDivided) 
			{ 
				for (int i = 0; i < 8 ; i++)
				{			
					if( pCurNode->pSubNodes[i] != NULL)
					{
						Vector3 vSubCenter =  pCurNode->GetSubNodeCenter(i);
						Aabb	aabbSubNode(Vector3(vSubCenter.x-fWidth/4,vSubCenter.y-fWidth/4,vSubCenter.z-fWidth/4),
											Vector3(vSubCenter.x+fWidth/4,vSubCenter.y+fWidth/4,vSubCenter.z+fWidth/4));
						if ( aabbSubNode.Contains(point))
						{
							pCurNode = pCurNode->pSubNodes[i];
							goto _Target1;
						}		
					}		
				}
				_Target1:;

				if(pCurNode == this)
					return NULL;
			} 
		}

		return pCurNode;
	}
}