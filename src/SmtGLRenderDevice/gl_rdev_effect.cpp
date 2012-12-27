#include "gl_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
namespace Smt_3Drd
{//light stuff
	long SmtGLRenderDevice::SetLight(int index,SmtLight *pLight)
	{
		GLenum gl_index = GL_LIGHT0 + index;
		if (gl_index < GL_LIGHT7 && gl_index > GL_LIGHT0)
		{
			if(pLight == NULL)
				glDisable(gl_index);
			else
			{
				LIGHTTYPE type = pLight->GetType();

				switch (type)
				{
				case LGT_SPOT:
					glLightf( gl_index, GL_SPOT_CUTOFF, pLight->GetCutoffAngle() );
					glLightf( gl_index, GL_SPOT_EXPONENT, pLight->GetExponent());
					break;
				default:
					glLightf( gl_index, GL_SPOT_CUTOFF, 180.0 );
					break;
				}

				// Color
				glLightfv(gl_index, GL_DIFFUSE,pLight->GetDiffuseValue().c);
				glLightfv(gl_index, GL_SPECULAR, pLight->GetSpecularValue().c);
				glLightfv(gl_index, GL_AMBIENT, pLight->GetAmbientValue().c);

				//position and direction
				glLightfv(gl_index, GL_POSITION, pLight->GetPosition().v);
				glLightfv(gl_index, GL_SPOT_DIRECTION, pLight->GetDirection().v);

				// Attenuation
				glLightf(gl_index, GL_CONSTANT_ATTENUATION, pLight->GetAttenuationConstant());
				glLightf(gl_index, GL_LINEAR_ATTENUATION, pLight->GetAttenuationLinear());
				glLightf(gl_index, GL_QUADRATIC_ATTENUATION, pLight->GetAttenuationQuadric());
				// Enable in the scene
				glEnable(gl_index);

				glEnable(GL_SPECULAR);
			}
		}

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetAmbientLight(const SmtColor &clr)
	{
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,clr.c);

		return SMT_ERR_NONE;
	}

	//texture
	long SmtGLRenderDevice::SetTexture(SmtTexture *pTex)
	{
		if(NULL != pTex)
			pTex->Use();
		else
			glDisable( GL_TEXTURE_2D );

		return SMT_ERR_NONE;
	}

	//materail
	long SmtGLRenderDevice::SetMaterial(SmtMaterial *pMat)
	{
		if (NULL != pMat)
		{
			glMaterialfv(GL_FRONT,GL_AMBIENT,pMat->GetAmbientValue().c);
			glMaterialfv(GL_FRONT,GL_SPECULAR,pMat->GetSpecularValue().c);
			glMaterialfv(GL_FRONT,GL_DIFFUSE,pMat->GetDiffuseValue().c);
			glMaterialfv(GL_FRONT,GL_EMISSION,pMat->GetEmissiveValue().c);

			float fShinV = pMat->GetShininessValue();
			glMaterialfv(GL_FRONT,GL_SHININESS,&(fShinV));
			glEnable(GL_COLOR_MATERIAL);
		}
		else
			glDisable(GL_COLOR_MATERIAL);

		return SMT_ERR_NONE;
	}

	//set fog
	long SmtGLRenderDevice::SetFog(FogMode mode, const SmtColor& col, float density, float start, float end)
	{	
		GLint fogMode;
		switch (mode)
		{
		case FM_EXP:
			fogMode = GL_EXP;
			break;
		case FM_EXP2:
			fogMode = GL_EXP2;
			break;
		case FM_LINEAR:
			fogMode = GL_LINEAR;
			break;
		default:
			// Give up on it
			glDisable(GL_FOG);
			return SMT_ERR_NONE;
		}

		glEnable(GL_FOG);
		glFogi(GL_FOG_MODE, fogMode);
		glFogfv(GL_FOG_COLOR, col.c);
		glFogf(GL_FOG_DENSITY, density);
		glFogf(GL_FOG_START, start);
		glFogf(GL_FOG_END, end);
		glHint(GL_FOG_HINT, GL_NICEST);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	// aactivate additive blending
	long SmtGLRenderDevice::SetBlending(bool bBlending)
	{
		if (m_bBlending == bBlending) 
			return SMT_ERR_NONE;

		m_bBlending = bBlending;

		if (!m_bBlending) 
		{
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glEnable(GL_ALPHA_TEST);
		}
		else
		{
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_ALPHA_TEST);
		}

		return SMT_ERR_NONE;
	}

	// activate backface culling
	long SmtGLRenderDevice::SetBackfaceCulling(RenderStateValue rsv )
	{
		if (rsv == RSV_CULL_CW) 
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_CCW);
		}
		else if (rsv == RSV_CULL_CCW)
		{
			if (rsv == RSV_CULL_CW) 
			{
				glEnable(GL_CULL_FACE);
				glCullFace(GL_CW);
			}
		}
		else
			glDisable(GL_CULL_FACE);

		return SMT_ERR_NONE;
	}

	// activate stencil buffer
	long SmtGLRenderDevice::SetStencilBufferMode(RenderStateValue rsv,DWORD dw)
	{
		switch (rsv) 
		{
		  case RSV_DEPTHBIAS:
			
			  // function modes and values
		  case RSV_CMP_ALWAYS:
			  m_unStencilCmp = GL_ALWAYS;
			  break;

		  case RSV_CMP_LESSEQUAL:
			  m_unStencilCmp = GL_LEQUAL;
			  break;

		  case RSV_STENCIL_MASK:
			 m_unStencilMask = dw;
			
			  break;
		  case RSV_STENCIL_WRITEMASK:
			 m_unStencilWriteMask = dw;
			  break;

		  case RSV_STENCIL_REF:
			  m_nStencilRef = dw;
			  break;

			  // stencil test fails modes
		  case RSV_STENCIL_FAIL_DECR:
			  m_unOpStencilFail = GL_DECR;
			  
			  break;
		  case RSV_STENCIL_FAIL_INCR:
			  m_unOpStencilFail = GL_INCR;
			 
			  break;
		  case RSV_STENCIL_FAIL_KEEP:
			  m_unOpStencilFail = GL_KEEP;
			  break;

			  // stencil test passes but z test fails modes
		  case RSV_STENCIL_ZFAIL_DECR:
			  m_unOpStencilZFail = GL_DECR;
			  break;

		  case RSV_STENCIL_ZFAIL_INCR:
			  m_unOpStencilZFail = GL_INCR;
			  break;

		  case RSV_STENCIL_ZFAIL_KEEP:
			  m_unOpStencilZFail = GL_KEEP;
			  break;

			  // stencil test passes modes
		  case RSV_STENCIL_PASS_DECR:
			  m_unOpStencilZPass = GL_DECR;
			  break;

		  case RSV_STENCIL_PASS_INCR:
			  m_unOpStencilZPass = GL_INCR;
			  break;

		  case RSV_STENCIL_PASS_KEEP:
			  m_unOpStencilZPass = GL_KEEP;
			  break;

		} 

		glStencilMask(m_unStencilWriteMask);
		glStencilFunc(m_unStencilCmp, m_nStencilRef,m_unStencilMask);
		glStencilOp(m_unOpStencilFail,m_unOpStencilZFail,m_unOpStencilZPass);

		return SMT_ERR_NONE;
	}

	// activate depth buffer
	long SmtGLRenderDevice::SetDepthBufferMode(RenderStateValue rsv)
	{
		if (rsv == RSV_DEPTH_READWRITE) 
		{
			glDepthMask(1);
			glEnable(GL_DEPTH_TEST);
		}

		else if (rsv == RSV_DEPTH_READONLY) 
		{
			glDepthMask(0);
			glEnable(GL_DEPTH_TEST);
		}

		else if (rsv == RSV_DEPTH_NONE)
		{
			glEnable(GL_DEPTH_TEST);
		}
		return SMT_ERR_NONE;
	}

	// activate wireframe mode
	long SmtGLRenderDevice::SetShadeMode(RenderStateValue rsv,float f,const SmtColor &clr)
	{
		if (rsv == RSV_SHADE_TRIWIRE)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glEnable(GL_FLAT);
		}
		else
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_SMOOTH);
		}

		if (rsv == RSV_SHADE_POINTS) 
		{
			if (f > 0.0f)
			{
				glPointSize(f);
			}
			else 
			{
				glPointSize(1.);
			}
		}

		return SMT_ERR_NONE;
	}
}