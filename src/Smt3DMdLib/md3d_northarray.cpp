#include "md3d_northarray.h "

namespace Smt_3DModel
{
	SmtNorthArray::SmtNorthArray(float initAngle,float fWinH,SmtPerspCamera* pCamera)
	{
		m_fNorthPtAngle = initAngle;
		m_pCamera       = pCamera;
		m_fWinH         = fWinH;
		m_nFontClock    = 0;
		m_pVBClockPan   = NULL;
		m_pVBClockArray = NULL;
	}

	SmtNorthArray::~SmtNorthArray()
	{
		Destroy();
	}

	long SmtNorthArray::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		Smt3DObject::Init(vPos,matMaterial,szTexName);

		return SMT_ERR_NONE;
	}

	long SmtNorthArray::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		//////////////////////////////////////////////////////////////////////////
		p3DRenderDevice->CreateFont("Arial",16,0,FW_BOLD,TRUE,FALSE,FALSE,10,m_nFontClock);

		//////////////////////////////////////////////////////////////////////////
		float R = m_fWinH*7/16;
		m_pVBClockPan   = p3DRenderDevice->CreateVertexBuffer(361,VF_XYZ|VF_DIFFUSE,true);

		if (SMT_ERR_NONE == m_pVBClockPan->Lock())
		{
			for (int i = 0 ; i < 361 ; i ++)
			{
				float x,y;
				x = R*cos(DEG2RAD(i))+m_fWinH*17/32;
				y = R*sin(DEG2RAD(i))+m_fWinH*17/32;
				m_pVBClockPan->Vertex(x,y,0);
				m_pVBClockPan->Diffuse(0.0,0.0,1.0,0.9);
			}

			m_pVBClockPan->Unlock();
		}

		//////////////////////////////////////////////////////////////////////////
		float L1 = m_fWinH*3/10,L2 = m_fWinH/5;
		m_pVBClockArray   = p3DRenderDevice->CreateVertexBuffer(4,VF_XYZ|VF_DIFFUSE,true);
		
		if (SMT_ERR_NONE == m_pVBClockArray->Lock())
		{
			float x,y;
			x = L2 * cos(DEG2RAD(120));//+m_fWinH*17/32;
			y = L2 * sin(DEG2RAD(120));//+m_fWinH*17/32;
			m_pVBClockArray->Vertex(x,y,0);
			m_pVBClockArray->Diffuse(0.0,.0,.0,1);

			x = L1 * cos(DEG2RAD(270));//+m_fWinH*17/32;
			y = L1 * sin(DEG2RAD(270));//+m_fWinH*17/32;
			m_pVBClockArray->Vertex(x,y,0);
			m_pVBClockArray->Diffuse(0.0,.0,.0,0);

			x = 0.;//m_fWinH*17/32;
			y = 0.;//m_fWinH*17/32;
			m_pVBClockArray->Vertex(x,y,0);
			m_pVBClockArray->Diffuse(0.0,.0,.0,1);

			x = L2 * cos(DEG2RAD(60));//+m_fWinH*17/32;
			y = L2 * sin(DEG2RAD(60));//+m_fWinH*17/32;
			m_pVBClockArray->Vertex(x,y,0);
			m_pVBClockArray->Diffuse(0.0,.0,.0,1);

			m_pVBClockArray->Unlock();
		}
		
		return SMT_ERR_NONE;
	}

	long SmtNorthArray::Destroy()
	{
		m_pCamera       = NULL;
		SMT_SAFE_DELETE(m_pVBClockPan);
		SMT_SAFE_DELETE(m_pVBClockArray);

		return SMT_ERR_NONE;
	}

	long SmtNorthArray::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		Vector3 vDir = m_pCamera->GetTarget() - m_pCamera->GetEye();
		vDir.y = 0;
		vDir.Normalize();
		m_fNorthPtAngle = RAD2DEG(acosf(vDir.z));
		if (vDir.x < 0)
		{
			m_fNorthPtAngle = 360 - m_fNorthPtAngle;
		}
		return SMT_ERR_NONE;
	}


	long SmtNorthArray::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		DWORD dwLightOn = 0;
		Viewport3D viewport = p3DRenderDevice->GetViewport();
		Viewport3D viewportOrg = viewport;

		viewport.ulHeight = m_fWinH;
		viewport.ulWidth  = m_fWinH;

		p3DRenderDevice->SetViewport(viewport);
		
		//reset the modelview matrix
		p3DRenderDevice->MatrixLoadIdentity();

		//now render the current height map in the corner
		p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
		p3DRenderDevice->MatrixPush();

		p3DRenderDevice->MatrixLoadIdentity();
		p3DRenderDevice->SetOrtho(0., viewport.ulWidth, 0., viewport.ulHeight, -1., 1. );//set up an ortho screen
		p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);//select the modelview matrix

		SmtGPUStateManager *stateManager = p3DRenderDevice->GetStateManager();
		stateManager->SetLight(false);
		stateManager->Set2DTextures(false);
		
		//stateManager->SetBlending(true);
        DrawClock(p3DRenderDevice);
		DrawArray(p3DRenderDevice);
		//stateManager->SetBlending(false);
		stateManager->SetLight(true);
		stateManager->Set2DTextures(true);
	
		//go back to perspective mode
		p3DRenderDevice->MatrixModeSet(MM_PROJECTION);//select the projection matrix
		p3DRenderDevice->MatrixPop();                 //restore the old projection matrix
		p3DRenderDevice->MatrixModeSet(MM_MODELVIEW); //select the modelview matrix
		
		p3DRenderDevice->SetViewport(viewportOrg);

		return SMT_ERR_NONE;
	}

	void SmtNorthArray::DrawClock(LP3DRENDERDEVICE p3DRenderDevice)
	{
        float R = m_fWinH*3/8,x,y;
		//
		x = R*cos(DEG2RAD(0))+m_fWinH/2;
		y = R*sin(DEG2RAD(0))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(0.,1.,1.),"E");

		x = R*cos(DEG2RAD(270))+m_fWinH/2;
		y = R*sin(DEG2RAD(270))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(0.,1.,1.),"N");

		x = R*cos(DEG2RAD(180))+m_fWinH/2;
		y = R*sin(DEG2RAD(180))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(0.,1.,1.),"W");

		x = R*cos(DEG2RAD(90))+m_fWinH/2;
		y = R*sin(DEG2RAD(90))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(0.,1.,1.),"S");

		//
		x = R*cos(DEG2RAD(330))+m_fWinH/2;
		y = R*sin(DEG2RAD(330))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"2");

		x = R*cos(DEG2RAD(300))+m_fWinH/2;
		y = R*sin(DEG2RAD(300))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"1");

		x = R*cos(DEG2RAD(240))+m_fWinH/2;
		y = R*sin(DEG2RAD(240))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"11");

		x = R*cos(DEG2RAD(210))+m_fWinH/2;
		y = R*sin(DEG2RAD(210))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"10");

		x = R*cos(DEG2RAD(150))+m_fWinH/2;
		y = R*sin(DEG2RAD(150))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"8");

		x = R*cos(DEG2RAD(120))+m_fWinH/2;
		y = R*sin(DEG2RAD(120))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"7");

		x = R*cos(DEG2RAD(60))+m_fWinH/2;
		y = R*sin(DEG2RAD(60))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"5");

		x = R*cos(DEG2RAD(30))+m_fWinH/2;
		y = R*sin(DEG2RAD(30))+m_fWinH/2;
		p3DRenderDevice->DrawText(m_nFontClock,x,y,SmtColor(1.0,0.4,0.6),"4");

		//
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVBClockPan,0,360);
	}

	void SmtNorthArray::DrawArray(LP3DRENDERDEVICE p3DRenderDevice)
	{	
		p3DRenderDevice->DrawText(m_nFontClock,0, m_fWinH*3/32,SmtColor(0.4,1.0,0.6),"%.1f",360-m_fNorthPtAngle);
		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixTranslation( m_fWinH*17/32, m_fWinH*17/32, 0.f );
		p3DRenderDevice->MatrixRotation(m_fNorthPtAngle-180, 0, 0, 1 );	
		p3DRenderDevice->DrawPrimitives(PT_TRIANGLESTRIP,m_pVBClockArray,0,2);
		p3DRenderDevice->MatrixPop();
	}
}