#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Other functions
	long SmtD3DRenderDevice::SetClearColor(const SmtColor &clr)
	{
		m_clrBack = D3DCOLOR_COLORVALUE(clr.fRed,clr.fGreen,clr.fBlue,clr.fA);

		return SMT_ERR_NONE;
	}

	long  SmtD3DRenderDevice::SetDepthClearValue( float z )
	{
		m_fDepClearVal = z;
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetStencilClearValue( ulong s )
	{
		m_dwStencilClearVal = s;
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::Clear( DWORD flags )
	{
		DWORD d3dFlags=0;

		if ( flags & CLR_COLOR )
			d3dFlags |= D3DCLEAR_TARGET;

		if ( flags & CLR_ZBUFFER )
			d3dFlags |= D3DCLEAR_ZBUFFER;

		if ( flags & CLR_STENCIL )
			d3dFlags |= D3DCLEAR_STENCIL;

		m_pD3DDevice->Clear( 0, NULL, d3dFlags, m_clrBack, m_fDepClearVal, m_dwStencilClearVal );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetViewport(Viewport3D &viewport)
	{
		m_viewPort = viewport;

		if(m_viewPort.ulHeight==0 || m_viewPort.ulWidth == 0)
			return SMT_ERR_FAILURE;

		D3DVIEWPORT9 vp;
		vp.X      = viewport.ulX;
		vp.Y      = viewport.ulY;
		vp.Width  = viewport.ulWidth;
		vp.Height = viewport.ulHeight;
		vp.MinZ   = viewport.fZNear;
		vp.MaxZ   = viewport.fZFar;

		m_pD3DDevice->SetViewport( &vp ) ;

		return SMT_ERR_NONE;
	}

	long  SmtD3DRenderDevice::SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar)
	{
		D3DXMatrixOrthoOffCenterRH(&m_prjD3DMatrix,left,right,bottom,top,zNear,zFar);
		m_pD3DDevice->SetTransform(D3DTS_PROJECTION, &m_prjD3DMatrix);

		return SMT_ERR_NONE;
	}

	long  SmtD3DRenderDevice::SetPerspective( float fovy, float aspect, float zNear, float zFar)
	{
		D3DXMatrixPerspectiveFovRH(&m_prjD3DMatrix, D3DXToRadian(fovy), aspect,zNear,zFar);

		m_pD3DDevice->SetTransform(D3DTS_PROJECTION, &m_prjD3DMatrix);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetViewLookAt(Vector3 &vPos,Vector3 &vView,Vector3 &vUp)
	{
		D3DXMatrixLookAtRH(&m_viewD3DMatrix, 
			(const D3DXVECTOR3*)&vPos, 
			(const D3DXVECTOR3*)&vView, 
			(const D3DXVECTOR3*)&vUp);

		m_pD3DDevice->SetTransform(D3DTS_VIEW, &m_viewD3DMatrix);

		return SMT_ERR_NONE;
	}

	//calculator 3D pos by 2D pos
	long SmtD3DRenderDevice::Transform2DTo3D(Vector3 &vOrg,Vector3 &vDir,const lPoint &point)
	{
		return SMT_ERR_NONE;
	}

	//3d to 2d
	long SmtD3DRenderDevice::Transform3DTo2D(const Vector3 &ver3D,lPoint &point)
	{
		return SMT_ERR_NONE;
	}


	long SmtD3DRenderDevice::GetFrustum(SmtFrustum &smtFrustum)
	{
		return SMT_ERR_NONE;
	}
}