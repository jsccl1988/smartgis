#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
namespace Smt_3Drd
{//light stuff
	inline DWORD FtoDW( FLOAT f ) { return *((DWORD*)&f); }

	long SmtD3DRenderDevice::SetLight(int index,SmtLight *pLight)
	{
		D3DLIGHT9 d3dLight;

		memset(&d3dLight, 0, sizeof(D3DLIGHT9));

		switch (pLight->GetType()) 
		{
		case LGT_DIRECTIONAL:
			// set light values
			d3dLight.Type = D3DLIGHT_DIRECTIONAL;

			// copy light direction
			memcpy(&d3dLight.Direction, pLight->GetDirection().v,sizeof(float)*4);
			break;

		case LGT_POINT:
			// set light values
			d3dLight.Type  = D3DLIGHT_POINT;
			d3dLight.Range = 1/pLight->GetAttenuationLinear();
			d3dLight.Attenuation1 = 1.0f;

			memcpy(&d3dLight.Position, pLight->GetDirection().v,sizeof(float)*4);
			break;

		case LGT_SPOT:
			// set light values
			d3dLight.Type     = D3DLIGHT_SPOT;
			d3dLight.Range    = pLight->GetRange();
			d3dLight.Falloff  = 1.0f;
			d3dLight.Theta    = pLight->GetThetaAngle();
			d3dLight.Phi      = pLight->GetPhiAngle();

			d3dLight.Attenuation0 = pLight->GetAttenuationConstant();
			d3dLight.Attenuation1 = pLight->GetAttenuationLinear();
			d3dLight.Attenuation2 = pLight->GetAttenuationQuadric();

			memcpy(&d3dLight.Position, pLight->GetPosition().v,sizeof(float)*4);
			memcpy(&d3dLight.Direction, pLight->GetDirection().v,sizeof(float)*4);
			break;
		} 

		memcpy(&d3dLight.Ambient, pLight->GetAmbientValue().c,sizeof(float)*4);
		memcpy(&d3dLight.Diffuse, pLight->GetDiffuseValue().c,sizeof(float)*4);
		memcpy(&d3dLight.Specular, pLight->GetSpecularValue().c,sizeof(float)*4);

		m_pD3DDevice->SetLight(index, &d3dLight);
		m_pD3DDevice->LightEnable(index, TRUE);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetAmbientLight(const SmtColor &clr)
	{
		int nRed   = (int)(clr.fRed * 255.0f);
		int nGreen = (int)(clr.fGreen * 255.0f);
		int nBlue  = (int)(clr.fBlue * 255.0f);

		m_pD3DDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(nRed, nGreen, nBlue));

		return SMT_ERR_NONE;
	}

	//texture
	long SmtD3DRenderDevice::SetTexture(SmtTexture *pTex)
	{
		return SMT_ERR_NONE;
	}

	//materail
	long SmtD3DRenderDevice::SetMaterial(SmtMaterial *pMat)
	{
		if (NULL != pMat)
		{
			//...
			D3DMATERIAL9 d3dMat;

			memcpy(&(d3dMat.Ambient),pMat->GetAmbientValue().c,sizeof(float)*4);
			memcpy(&(d3dMat.Diffuse),pMat->GetAmbientValue().c,sizeof(float)*4);
			memcpy(&(d3dMat.Emissive),pMat->GetEmissiveValue().c,sizeof(float)*4);
			memcpy(&(d3dMat.Specular),pMat->GetSpecularValue().c,sizeof(float)*4);
			d3dMat.Power = pMat->GetShininessValue();

			if (SUCCEEDED(m_pD3DDevice->SetMaterial(&d3dMat)) )
			{
				return SMT_ERR_NONE;
			}

			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	//set fog
	long SmtD3DRenderDevice::SetFog(FogMode mode, const SmtColor& clr, float density, float start, float end)
	{	
		//...
		int nRed   = (int)(clr.fRed * 255.0f);
		int nGreen = (int)(clr.fGreen * 255.0f);
		int nBlue  = (int)(clr.fBlue * 255.0f);
		DWORD dwMode = 0;

		switch (mode)
		{
		case FM_EXP:
			dwMode = D3DFOG_EXP;
			break;
		case FM_EXP2:
			dwMode = D3DFOG_EXP2;
			break;
		case FM_LINEAR:
			dwMode = D3DFOG_LINEAR;
			break;
		default:
			// Give up on it
			m_pD3DDevice->SetRenderState(D3DRS_FOGENABLE,FALSE); 
			return SMT_ERR_NONE;
		}

		m_pD3DDevice->SetRenderState(D3DRS_FOGENABLE,TRUE);
		m_pD3DDevice->SetRenderState(D3DRS_FOGVERTEXMODE, dwMode);
		m_pD3DDevice->SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_XRGB(nRed, nGreen, nBlue));


		m_pD3DDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD*)(&start));
		m_pD3DDevice->SetRenderState(D3DRS_FOGEND, *(DWORD*)(&end));

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	// aactivate additive blending
	long SmtD3DRenderDevice::SetBlending(bool bBlending)
	{
		if (m_bBlending == bBlending) 
			return SMT_ERR_NONE;

		m_bBlending = bBlending;

		if (!m_bBlending) 
		{
			m_pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			m_pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			m_pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		}
		return SMT_ERR_NONE;
	}

	// activate backface culling
	long SmtD3DRenderDevice::SetBackfaceCulling(RenderStateValue rsv )
	{
		if (rsv == RSV_CULL_CW) 
			m_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

		else if (rsv == RSV_CULL_CCW) 
			m_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

		else if (rsv == RSV_CULL_NONE) 
			m_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		return SMT_ERR_NONE;
	}

	// activate stencil buffer
	long SmtD3DRenderDevice::SetStencilBufferMode(RenderStateValue rsv,DWORD dw)
	{
		switch (rsv) 
		{
		case RSV_DEPTHBIAS:
			m_pD3DDevice->SetRenderState(D3DRS_DEPTHBIAS, dw);

			// function modes and values
		case RSV_CMP_ALWAYS:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
			break;
		case RSV_CMP_LESSEQUAL:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
			break;
		case RSV_STENCIL_MASK:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILMASK, dw);
			break;
		case RSV_STENCIL_WRITEMASK:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILWRITEMASK, dw);
			break;
		case RSV_STENCIL_REF:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILREF, dw);
			break;

			// stencil test fails modes
		case RSV_STENCIL_FAIL_DECR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_DECR);
			break;
		case RSV_STENCIL_FAIL_INCR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_INCR);
			break;
		case RSV_STENCIL_FAIL_KEEP:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
			break;

			// stencil test passes but z test fails modes
		case RSV_STENCIL_ZFAIL_DECR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
			break;
		case RSV_STENCIL_ZFAIL_INCR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
			break;
		case RSV_STENCIL_ZFAIL_KEEP:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
			break;

			// stencil test passes modes
		case RSV_STENCIL_PASS_DECR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_DECR);
			break;
		case RSV_STENCIL_PASS_INCR:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
			break;
		case RSV_STENCIL_PASS_KEEP:
			m_pD3DDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
			break;
		} // switch

		return SMT_ERR_NONE;
	}

	// activate depth buffer
	long SmtD3DRenderDevice::SetDepthBufferMode(RenderStateValue rsv)
	{
		if (rsv == RSV_DEPTH_READWRITE) 
		{
			m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			m_pD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		}
		else if (rsv == RSV_DEPTH_READONLY) 
		{
			m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			m_pD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		}
		else if (rsv == RSV_DEPTH_NONE)
		{
			m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			m_pD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		}

		return SMT_ERR_NONE;
	}

	// activate wireframe mode
	long SmtD3DRenderDevice::SetShadeMode(RenderStateValue rsv,float f,const SmtColor &clr)
	{
		if (rsv == RSV_SHADE_TRIWIRE)
		{
			// only triangle wireframe ist set via d3d shade mode
			m_pD3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
			m_pD3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		}
		else
		{
			if (rsv != RSV_SHADE_SOLID) 
			{
				m_pD3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
			}
			m_pD3DDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
			m_pD3DDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		}

		if (rsv == RSV_SHADE_POINTS) 
		{
			if (f > 0.0f)
			{
				m_pD3DDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, TRUE);
				m_pD3DDevice->SetRenderState(D3DRS_POINTSCALEENABLE,  TRUE);
				m_pD3DDevice->SetRenderState(D3DRS_POINTSIZE,     FtoDW( f ));
				m_pD3DDevice->SetRenderState(D3DRS_POINTSIZE_MIN, FtoDW(0.00f));
				m_pD3DDevice->SetRenderState(D3DRS_POINTSCALE_A,  FtoDW(0.00f));
				m_pD3DDevice->SetRenderState(D3DRS_POINTSCALE_B,  FtoDW(0.00f));
				m_pD3DDevice->SetRenderState(D3DRS_POINTSCALE_C,  FtoDW(1.00f));
			}
			else 
			{
				m_pD3DDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
				m_pD3DDevice->SetRenderState(D3DRS_POINTSCALEENABLE,  FALSE);
			}
		}
		else 
		{
			m_pD3DDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
			m_pD3DDevice->SetRenderState(D3DRS_POINTSCALEENABLE,  FALSE);
		}

		return SMT_ERR_NONE;
	}
}