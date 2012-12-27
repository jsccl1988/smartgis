#include "bl3d_scene.h"
#include "smt_bas_struct.h "
#include "smt_api.h"
#include "smt_logmanager.h"
#include "smt_thread.h"
#include <algorithm>

namespace Smt_3DBase
{
	//////////////////////////////////////////////////////////////////////////
	SmtScene::SmtScene(void):m_p3DRenderDevice(NULL)
		,m_pTimer(NULL)
		,m_pCamera(NULL)
		,m_bOctTreeCreated(false)
		,m_bShowNodeBox(true)
		,m_nHelpInfoFont(0)
		,m_nTimerInfoFont(0)
		,m_nRenderInfoFont(0)
		,m_pNorthArray(NULL)
		,m_pSceneTree(NULL)
	{
		m_szHelpInfoBuf[0] = '\0';
		m_szRenderInfoBuf[0] = '\0';
	}

	SmtScene::~SmtScene(void)
	{
		vSmt3DObjectPtrs::iterator iter = m_v3DObjectPtrs.begin();
		while(iter != m_v3DObjectPtrs.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}

		SMT_SAFE_DELETE(m_pNorthArray);
		SMT_SAFE_DELETE(m_pSceneTree);
		SMT_SAFE_DELETE(m_pTimer);

		m_pCamera		  = NULL;
		m_p3DRenderDevice = NULL;
	}

	inline	void SmtScene::SetSceneCamera(SmtPerspCamera *pCamera) 
	{
		m_pCamera = pCamera;
		if (m_pNorthArray)
			m_pNorthArray->SetPerspCamera(m_pCamera);
	}

	long SmtScene::Setup()
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//
		m_pSceneTree	= new SmtSceneOctTree();
		m_pTimer		= new SmtTimer();
		m_pNorthArray	= new SmtNorthArray(90,120,NULL);
		if (m_pNorthArray->Init(Vector3(0,0,0),SmtMaterial()) == SMT_ERR_NONE)
		{
			m_pNorthArray->Create(m_p3DRenderDevice);
		}

		//
		m_pTimer->SetClock(0,0);

		//
		m_pSceneTree->SetShowNodeBox(m_bShowNodeBox);

		//////////////////////////////////////////////////////////////////////////
		//´´½¨×ÖÌå
		m_p3DRenderDevice->CreateFont("Calibri",0,0,FW_BOLD,FALSE,FALSE,FALSE,16,m_nRenderInfoFont);
		m_p3DRenderDevice->CreateFont("MS Reference Sans Serif",16,0,FW_BOLD,FALSE,FALSE,FALSE,16,m_nTimerInfoFont);
		m_p3DRenderDevice->CreateFont("Times New Roman",16,0,FW_BOLD,FALSE,FALSE,FALSE,10,m_nHelpInfoFont);

		pLog->LogMessage("SmtScene::Setup() is ok!");

		return SMT_ERR_NONE;
	}

	long SmtScene::Update()
	{
		if (m_pTimer && m_pCamera && m_pSceneTree)
		{
			//
			m_pTimer->Update();

			//
			sprintf(m_szHelpInfoBuf,"Move:Front:W  Left:A  Back:S  Right:D Eye : X%f  Y:%f   Z:%f ",m_pCamera->GetEye().x,m_pCamera->GetEye().y,m_pCamera->GetEye().z);

			
			if (m_bOctTreeCreated)
			{
				m_pSceneTree->SetShowNodeBox(m_bShowNodeBox);

				m_pSceneTree->Update(m_p3DRenderDevice,m_pTimer->GetElapsed());

			}
			else
			{
				vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
				while(iter != m_v3DObjectPtrs.end())
				{
					if (NULL != (*iter))
					{
						(*iter)->Update(m_p3DRenderDevice,m_pTimer->GetElapsed());
					}
					iter++;
				}
			}		

			if (m_pNorthArray)
				m_pNorthArray->Update(m_p3DRenderDevice,m_pTimer->GetElapsed());
		}

		return SMT_ERR_NONE;
	}

	long SmtScene::Render(void)
	{
		if (NULL != m_pSceneTree && NULL != m_p3DRenderDevice && m_pTimer != NULL)
		{
			if (m_pCamera)
				m_pCamera->Apply();

			//////////////////////////////////////////////////////////////////////////
			//scene object
			if (m_bOctTreeCreated)
			{
				static char szBuf[TEMP_BUFFER_SIZE];
				
				if (m_pSceneTree->IsVisible())
				{
					m_pSceneTree->Render(m_p3DRenderDevice);
					m_pSceneTree->GetDebugString(szBuf,TEMP_BUFFER_SIZE);
					sprintf(m_szRenderInfoBuf,"Fps%.3f\t%s",m_pTimer->GetFPS(),szBuf);
				}
				else
					sprintf(m_szRenderInfoBuf,"Fps%.3f\t",m_pTimer->GetFPS());
			}
			else
			{
				sprintf(m_szRenderInfoBuf,"Fps%.3f\t",m_pTimer->GetFPS());

				vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
				while(iter != m_v3DObjectPtrs.end())
				{
					if (NULL != (*iter) && (*iter)->IsVisible())
					{
						(*iter)->Render(m_p3DRenderDevice);
					}
					iter++;
				}
			}

			//////////////////////////////////////////////////////////////////////////
			if (m_pNorthArray && m_pNorthArray->IsVisible())
				m_pNorthArray->Render(m_p3DRenderDevice);

			//////////////////////////////////////////////////////////////////////////
			//3d text 
			m_p3DRenderDevice->DrawText(m_nTimerInfoFont,0,0,0,SmtColor(0.,1.,0.),"(0,0,0)");

			//////////////////////////////////////////////////////////////////////////
			//2d text
			m_p3DRenderDevice->DrawText(m_nHelpInfoFont,10,24,SmtColor(0.,0.,1.),m_szHelpInfoBuf);
			m_p3DRenderDevice->DrawText(m_nRenderInfoFont,10,44,SmtColor(0.,1.,0.),m_szRenderInfoBuf);
			m_p3DRenderDevice->DrawText(m_nTimerInfoFont,10,64,SmtColor(1.,1.,0.),m_pTimer->GetClock());
		}

		return SMT_ERR_NONE;
	}
	//////////////////////////////////////////////////////////////////////////
	long SmtScene::Transform2DTo3D(Vector3 &vOrg,Vector3 &vTar,const lPoint &point)
	{
		if (m_pCamera)
			m_pCamera->Apply();

		if (NULL != m_p3DRenderDevice)
			m_p3DRenderDevice->Transform2DTo3D(vOrg,vTar,point);

		return SMT_ERR_NONE;
	}

	long SmtScene::Transform3DTo2D(const Vector3 &ver3D,lPoint &point)
	{
		if (m_pCamera)
			m_pCamera->Apply();

		if (NULL != m_p3DRenderDevice)
			m_p3DRenderDevice->Transform3DTo2D(ver3D,point);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtScene::CreateOctTreeSceneMgr(void)
	{
		m_pSceneTree->CreateOctTree(m_v3DObjectPtrs);
		m_aAbb = m_pSceneTree->m_aabbScene;
		m_bOctTreeCreated = true;
	}

	void SmtScene::Add3DObject(Smt3DObject* p3DObject)
	{
		if (NULL != p3DObject)
		{
			m_v3DObjectPtrs.push_back(p3DObject);
			m_aAbb.Merge(p3DObject->GetAabb());
			m_aAbb.vcCenter = (m_aAbb.vcMax+m_aAbb.vcMin)/2.;
			m_bOctTreeCreated = false;
		}
	}

	void SmtScene::Remove3DObject(int index)
	{
		vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
		while(iter != m_v3DObjectPtrs.end())
		{
			if (index == 0)
			{
				SMT_SAFE_DELETE(*iter);
				m_v3DObjectPtrs.erase(iter);
				CreateOctTreeSceneMgr();
				break;
			}

			++iter;
			--index;
		}
	}

	void SmtScene::Remove3DObject(Smt3DObject* p3DObject)
	{
		vSmt3DObjectPtrs::iterator iter; 
		iter = find(m_v3DObjectPtrs.begin(),m_v3DObjectPtrs.end(),p3DObject); 

		if(iter !=m_v3DObjectPtrs.end()) 
		{
			SMT_SAFE_DELETE(*iter);
			m_v3DObjectPtrs.erase(iter); 
			CreateOctTreeSceneMgr();
		}
	}

	Smt3DObject* SmtScene::Get3DObject(int index)
	{
		vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
		while(iter != m_v3DObjectPtrs.end())
		{
			if (index == 0)
			{
				return(*iter);
			}

			++iter;
			--index;
		}

		return NULL;
	}

	const Smt3DObject* SmtScene::Get3DObject(int index) const
	{
		vSmt3DObjectPtrs ::const_iterator iter = m_v3DObjectPtrs.begin();
		while(iter != m_v3DObjectPtrs.end())
		{
			if (index == 0)
			{
				return(*iter);
			}

			++iter;
			--index;
		}

		return NULL;
	}

	void SmtScene::Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs)
	{
		v3DObjectPtrs = m_v3DObjectPtrs;
	}

	long SmtScene::Select3DObject(vSmt3DObjectPtrs &vSelected3DObjects, lPoint point )
	{
		if (m_pCamera)
			m_pCamera->Apply();

		if (m_bOctTreeCreated)
		{
			if (NULL != m_pSceneTree &&  NULL != m_p3DRenderDevice)
				m_pSceneTree->Select3DObject(vSelected3DObjects,m_p3DRenderDevice,point);
		}
		else
		{
			vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
			while(iter != m_v3DObjectPtrs.end())
			{
				if (NULL != (*iter))
				{
					if ((*iter)->Select(m_p3DRenderDevice,point))
					{
						vSelected3DObjects.push_back(*iter);
					}
				}

				iter++;
			}
		}
		
		return SMT_ERR_NONE;
	}

	long SmtScene::TransModel3DObjects(Matrix&matTransform)
	{
		if (m_bOctTreeCreated)
		{
			if (NULL != m_pSceneTree)
				m_pSceneTree->ObjectModelMatrixMultiply(matTransform);
		}
		else
		{
			vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
			while(iter != m_v3DObjectPtrs.end())
			{
				if (NULL != (*iter))
				{
					(*iter)->ModelTransMatrixMultiply(matTransform);
				}
				iter++;
			}
		}

		return SMT_ERR_NONE;
	}

	long SmtScene::TransWorld3DObjects(Matrix&matTransform)
	{
		if (m_bOctTreeCreated)
		{
			if (NULL != m_pSceneTree)
				m_pSceneTree->ObjectWordlMatrixMultiply(matTransform);
		}
		else
		{
			vSmt3DObjectPtrs ::iterator iter = m_v3DObjectPtrs.begin();
			while(iter != m_v3DObjectPtrs.end())
			{
				if (NULL != (*iter))
				{
					(*iter)->WorldTransMatrixMultiply(matTransform);
				}
				iter++;
			}
		}

		return SMT_ERR_NONE;
	}
}