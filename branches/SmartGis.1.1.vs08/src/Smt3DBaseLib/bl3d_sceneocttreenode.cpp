#include "bl3d_sceneocttree.h"

namespace Smt_3DBase
{
	//////////////////////////////////////////////////////////////////////////
	//OctTreeNode
	//////////////////////////////////////////////////////////////////////////
	SmtSceneOctTreeNode::SmtSceneOctTreeNode()
	{
		pParentNode = NULL;
		memset(pSubNodes,NULL,sizeof(SmtSceneOctTreeNode*)*8);
		fWidth = 0;
		strCode = "";
		bSubDivided = false;
		nTargetCount = 0;
	}

	SmtSceneOctTreeNode::~SmtSceneOctTreeNode()
	{
		v3DObjectPtrs.clear();

		for (int i = 0; i< 8; i++)
		{
			SMT_SAFE_DELETE(pSubNodes[i]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSceneOctTreeNode::CreateNode(vSmt3DObjectPtrs &v3DObjectPtrs,int nTarget,Vector3 vCenter,float width)
	{
		fWidth = width;
		vCenterPos = vCenter;
		if( (nTarget > g_nSceneMaxTargets) && (g_nSceneCurrentSubdivision < g_nSceneMaxSubdivision) )
		{
			bSubDivided = true;

			vector<bool> vBDealedTarget(nTarget);
			for (int j = 0; j < nTarget;j++)
			{
				vBDealedTarget[j] = false;
			}

			vector< vector<bool> > vvbInsideNode;
			vvbInsideNode.resize(8);
			for (int i = 0 ; i < 8; i++)
			{
				vector<bool> &vBInside =  vvbInsideNode[i];
				vBInside.resize(nTarget);	
				for (int j = 0; j < nTarget;j++)
				{
					vBInside[j] = false;
				}
			}

			for (int i = 0; i < 8 ; i++)
			{
				Vector3 vSubCenter =  GetSubNodeCenter(i);
				vector<bool> &vBInsideNode =  vvbInsideNode[i];
				Aabb	aabbSubNode(Vector4(vSubCenter.x-fWidth/4,vSubCenter.y-fWidth/4,vSubCenter.z-fWidth/4),
									Vector4(vSubCenter.x+fWidth/4,vSubCenter.y+fWidth/4,vSubCenter.z+fWidth/4));

				vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
				int iTargetIndex = 0;
				while(iter != v3DObjectPtrs.end())
				{
					if (NULL != (*iter))
					{
						if (!vBDealedTarget[iTargetIndex] && aabbSubNode.Contains((*iter)->GetAabb().vcCenter))
						{
							vBDealedTarget[iTargetIndex] = true;
							vBInsideNode[iTargetIndex] = true;
						}
					}
					iter ++;
					iTargetIndex++;
				}
			}

			int nTargetNum[8] ;

			for (int i = 0 ; i < 8; i++)
			{
				vector<bool> &vBInside =  vvbInsideNode[i];
				nTargetNum[i] = 0;
				for (int j = 0; j < nTarget;j++)
				{
					if (vBInside[j])
						nTargetNum[i]++;	 
				}
			}


			//创建子节点
			for (int i = 0; i < 8 ; i++)
			{
				vector<bool> &vBInsideNode =  vvbInsideNode[i];
				this->CreateSubNode(this,pSubNodes[i],v3DObjectPtrs,vBInsideNode,nTargetNum[i],i);
			}
		}
		else
		{
			this->bSubDivided = false;
			this->v3DObjectPtrs = v3DObjectPtrs;
			this->nTargetCount = v3DObjectPtrs.size();
			g_nSceneTotalLeafNode ++;
		}

		return SMT_ERR_NONE;
	}

	Vector3 SmtSceneOctTreeNode::GetSubNodeCenter(int nSubID)
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

	void SmtSceneOctTreeNode::CreateSubNode(SmtSceneOctTreeNode*pParent,SmtSceneOctTreeNode*&pSub,vSmt3DObjectPtrs& v3DObjectPtrs,vector<bool> vbInSubNode,int nTargets,int nSubID)
	{
		if (nTargets > 0)
		{
			pSub = new SmtSceneOctTreeNode();
			pSub->pParentNode = pParent;

			vSmt3DObjectPtrs vTmpTargetPtrs;

			//添加渲染对象
			int iTarget = 0;
			vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
			while(iter != v3DObjectPtrs.end())
			{
				if (vbInSubNode[iTarget])
				{
					vTmpTargetPtrs.push_back((*iter));
				}
				iter++;
				iTarget++;
			}

			g_nSceneCurrentSubdivision++;
			pSub->CreateNode(vTmpTargetPtrs,nTargets,pParent->GetSubNodeCenter(nSubID),pParent->fWidth/2.);
			g_nSceneCurrentSubdivision--;

			vTmpTargetPtrs.clear();
		}
	}
	void SmtSceneOctTreeNode::UpdateNodeObject(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		//更新子节点
		if (bSubDivided)
		{
			for (int i = 0; i < 8 ; i++)
			{
				if (pSubNodes[i])
					pSubNodes[i]->UpdateNodeObject(p3DRenderDevice,fElapsed);
			}
		}
		else
		{
			//更新本节点对象
			vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
			while(iter != v3DObjectPtrs.end())
			{
				if (NULL != (*iter))
				{
					(*iter)->Update(p3DRenderDevice,fElapsed);
					g_nSceneCurRenderTarget ++;
				}
				iter++;
			}
		}
	}

	void SmtSceneOctTreeNode::RenderNodeObject(LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,bool bShowOctNodeBox)
	{
		if (smtFrustum.IsCubeIn(vCenterPos.x,vCenterPos.y,vCenterPos.z,fWidth/2.))
		{
			//渲染本节点节点
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
				vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
				while(iter != v3DObjectPtrs.end())
				{
					if (NULL != (*iter) && (*iter)->IsVisible())
					{
						(*iter)->Render(p3DRenderDevice);
						g_nSceneCurRenderTarget ++;
					}
					iter++;
				}
			}
		}
	}

	void SmtSceneOctTreeNode::SelectNodeObject(vSmt3DObjectPtrs &vSelected3DObjects, LP3DRENDERDEVICE p3DRenderDevice,SmtFrustum &smtFrustum,const lPoint& point )
	{
		if (smtFrustum.IsCubeIn(vCenterPos.x,vCenterPos.y,vCenterPos.z,fWidth/2.))
		{
			//拾取子节点
			if (bSubDivided)
			{
				for (int i = 0; i < 8 ; i++)
				{
					if (pSubNodes[i])
						pSubNodes[i]->SelectNodeObject(vSelected3DObjects,p3DRenderDevice,smtFrustum,point);
				}
			}
			else
			{
				//拾取本节点对象
				vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
				while(iter != v3DObjectPtrs.end())
				{
					if ((*iter)->Select(p3DRenderDevice,point))
					{
						vSelected3DObjects.push_back(*iter);
					} 
					iter ++;
				}
			}
		}
	}

	void SmtSceneOctTreeNode::NodeObjectModelMatrixMultiply(Matrix&matTransform)
	{
		//矩阵相乘子节点
		if (bSubDivided)
		{
			for (int i = 0; i < 8 ; i++)
			{
				if (pSubNodes[i])
					pSubNodes[i]->NodeObjectModelMatrixMultiply(matTransform);
			}
		}
		else
		{
			//相乘本节点对象
			vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
			while(iter != v3DObjectPtrs.end())
			{
				if (NULL != (*iter) && (*iter)->IsVisible())
				{
					(*iter)->ModelTransMatrixMultiply(matTransform);
					g_nSceneCurRenderTarget ++;
				}
				iter++;
			}
		}
	}

	void SmtSceneOctTreeNode::NodeObjectWorldMatrixMultiply(Matrix&matTransform)
	{
		//矩阵相乘子节点
		if (bSubDivided)
		{
			for (int i = 0; i < 8 ; i++)
			{
				if (pSubNodes[i])
					pSubNodes[i]->NodeObjectWorldMatrixMultiply(matTransform);
			}
		}
		else
		{
			//相乘本节点对象
			vSmt3DObjectPtrs ::iterator iter = v3DObjectPtrs.begin();
			while(iter != v3DObjectPtrs.end())
			{
				if (NULL != (*iter) && (*iter)->IsVisible())
				{
					(*iter)->WorldTransMatrixMultiply(matTransform);
					g_nSceneCurRenderTarget ++;
				}
				iter++;
			}
		}
	}

	bool SmtSceneOctTreeNode::IsInOctNodeAabbBox(const Vector3& point)
	{
		Aabb	aabbNode(Vector3(this->vCenterPos.x-this->fWidth/2,this->vCenterPos.y-this->fWidth/2,this->vCenterPos.z-this->fWidth/2),
						 Vector3(this->vCenterPos.x+this->fWidth/2,this->vCenterPos.y+this->fWidth/2,this->vCenterPos.z+this->fWidth/2));
		if (aabbNode.Contains(point))
		{
			return true;
		}	
		else 
			return false;
	}

	SmtSceneOctTreeNode* SmtSceneOctTreeNode::FindMinBoxOctNode(const Ray &ray)
	{
		float   f;
		Aabb	aabbNode(Vector3(this->vCenterPos.x-this->fWidth/2,this->vCenterPos.y-this->fWidth/2,this->vCenterPos.z-this->fWidth/2),
						 Vector3(this->vCenterPos.x+this->fWidth/2,this->vCenterPos.y+this->fWidth/2,this->vCenterPos.z+this->fWidth/2));

		if (!aabbNode.Intersects(ray,&f))
			return NULL;
		 
		SmtSceneOctTreeNode *pCurNode = this;

		while (NULL != pCurNode && pCurNode->bSubDivided) 
		{ 
			for (int i = 0; i < 8 ; i++)
			{			
				if( pCurNode->pSubNodes[i] != NULL)
				{
					if (NULL != pCurNode->pSubNodes[i]->FindMinBoxOctNode(ray))
					{
						pCurNode = pCurNode->pSubNodes[i];
						goto _Target1;
					}		
				}		
			}
			_Target1:;
		}

		return pCurNode;
	}

	SmtSceneOctTreeNode* SmtSceneOctTreeNode::FindMinBoxOctNode(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
	{
		Vector3 vOrg,vDir;
		Ray ray;
		p3DRenderDevice->Transform2DTo3D(vOrg,vDir,point);
		ray.Set(vOrg,vDir);

		return FindMinBoxOctNode(ray);
	}
}