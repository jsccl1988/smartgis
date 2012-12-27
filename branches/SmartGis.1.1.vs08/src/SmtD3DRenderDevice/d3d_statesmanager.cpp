#include "d3d_statesmanager.h"

namespace Smt_3Drd
{
	/**
	Map to convert D3D consts to engine's and vice versa
	*/
	const static int g_D3DToEngineMap[][2] =
	{
		{D3DCMP_LESS, CMP_LESS}, {D3DCMP_LESSEQUAL, CMP_LEQUAL}, {D3DCMP_GREATER, CMP_GREATER}, {D3DCMP_GREATEREQUAL, CMP_GEQUAL}, {D3DCMP_EQUAL, CMP_EQUAL},
		{D3DBLEND_ZERO, BF_ZERO}, {D3DBLEND_ONE, BF_ONE}, {D3DBLEND_SRCCOLOR, BF_SRC_COLOR}, {D3DBLEND_DESTCOLOR, BF_DST_COLOR},
		{D3DBLEND_INVDESTCOLOR, BF_ONE_MINUS_DST_COLOR}, {D3DBLEND_SRCALPHA, BF_SRC_ALPHA},
		{D3DBLEND_INVSRCALPHA, BF_ONE_MINUS_SRC_ALPHA}, {D3DBLEND_DESTALPHA, BF_DST_ALPHA},
		{D3DBLEND_INVDESTALPHA, BF_ONE_MINUS_DST_ALPHA}, {D3DBLEND_SRCALPHASAT, BF_SRC_ALPHA_SATURATE}
	};

	/* Indexes of map tables for different constants */
	const static int COMPARISON_TABLE              = 0;
	const static int TEXTURE_FILTER_TABLE          = 1;
	const static int TEXTURE_COORD_WRAP_MODE_TABLE = 2;
	const static int TEXTURE_ENVIRONMENT_TABLE     = 3;
	const static int FACE_TABLE                    = 4;
	const static int POLYGON_MODE_TABLE            = 5;
	const static int BLEND_FACTOR_TABLE            = 6;

	const static int g_ComparisonTable[] = { D3DCMP_LESS, D3DCMP_LESSEQUAL, D3DCMP_GREATER, D3DCMP_GREATEREQUAL, D3DCMP_EQUAL };

	const static int g_TextureFilterTable[] =
	{ D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC};

	const static int g_TextureCoordWrapModeTable[] = {D3DTADDRESS_CLAMP, D3DTADDRESS_WRAP, D3DTADDRESS_BORDER };

	const static int g_TextureEnvironmentTable[] = { D3DSTENCILOP_REPLACE, D3DSTENCILOP_ZERO, D3DSTENCILOP_DECR, D3DBLENDOP_REVSUBTRACT, D3DBLENDOP_ADD };

	const static int g_FaceTable[] = { D3DCULL_CW, D3DCULL_CCW, D3DCULL_NONE };

	const static int g_PolygonModeTable[] = { D3DFILL_POINT, D3DFILL_WIREFRAME, D3DFILL_SOLID };

	const static int g_BlendFactorTable[] =
	{
		D3DBLEND_ZERO, D3DBLEND_ONE, D3DBLEND_SRCCOLOR, D3DBLEND_DESTCOLOR,
		D3DBLEND_INVDESTCOLOR, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCCOLOR,
		D3DBLEND_DESTALPHA, D3DBLEND_INVDESTALPHA, D3DBLEND_SRCALPHASAT
	};

	const static int* g_EngineToD3DMap[] =
	{
		g_ComparisonTable,
		g_TextureFilterTable,
		g_TextureCoordWrapModeTable,
		g_TextureEnvironmentTable,
		g_FaceTable,
		g_PolygonModeTable,
		g_BlendFactorTable
	};

	const static int g_TableSize[] =
	{
		sizeof(g_ComparisonTable) / sizeof(int),
		sizeof(g_TextureFilterTable) / sizeof(int),
		sizeof(g_TextureCoordWrapModeTable) / sizeof(int),
		sizeof(g_TextureEnvironmentTable) / sizeof(int),
		sizeof(g_FaceTable) / sizeof(int),
		sizeof(g_PolygonModeTable) / sizeof(int),
		sizeof(g_BlendFactorTable) / sizeof(int)
	};

	const static int g_TablesCount = sizeof(g_EngineToD3DMap) / sizeof(int*);

	/**
	Default constructor.
	*/
	SmtD3DGPUStateManager::SmtD3DGPUStateManager(IDirect3DDevice9	*pD3DDevice):m_pD3DDevice(pD3DDevice)
	{
		for (int i = 0; i < g_TablesCount; i++)
		{
			m_D3DToDev[g_D3DToEngineMap[i][0]] = g_D3DToEngineMap[i][1];
		}
	}

	int SmtD3DGPUStateManager::ConvertD3DEnum(uint value)
	{
		return m_D3DToDev[(int)value];
	}

	uint SmtD3DGPUStateManager::ConvertToD3DEnum(uint tableIndex, uint value)
	{
		if (tableIndex < g_TablesCount &&
			value < g_TableSize[tableIndex])
		{
			return g_EngineToD3DMap[tableIndex][value];
		}

		return -1;
	}

	SmtAlphaTestState SmtD3DGPUStateManager::GetAlphaTestState()
	{
		SmtAlphaTestState newState;
		int func = 0;
		float ref = 0;

		/*glGetIntegerv(GL_ALPHA_TEST_FUNC, &func);
		glGetFloatv(GL_ALPHA_TEST_REF, &ref);

		newState.bEnabled = glIsEnabled(GL_ALPHA_TEST);
		newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));
		newState.fRefValue = ref;*/

		return newState;
	}

	long SmtD3DGPUStateManager::SetAlphaTestState(SmtAlphaTestState &state)
	{
		if (SMT_ERR_NONE == SetAlphaTest(state.bEnabled) &&
			SMT_ERR_NONE == SetAlphaTestFunc(state.cmpFunc, state.fRefValue))
		{
			return SMT_ERR_NONE;
		}
		
		return SMT_ERR_FAILURE;
	}

	long SmtD3DGPUStateManager::SetAlphaTest(bool enabled)
	{
		if (enabled == true)
		{
			//glEnable(GL_ALPHA_TEST);
		}
		else
		{
			//glDisable(GL_ALPHA_TEST);
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetAlphaTestFunc(Comparison func, float ref)
	{
		//glAlphaFunc(ConvertToGLEnum(COMPARISON_TABLE, func), ref);

		return SMT_ERR_NONE;
	}

	SmtDepthTestState SmtD3DGPUStateManager::GetDepthTestState()
	{
		SmtDepthTestState newState;
		bool depthMask;
		int func = 0;

		/*glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
		glGetIntegerv(GL_DEPTH_FUNC, &func);

		newState.bEnabled = glIsEnabled(GL_DEPTH_TEST);
		newState.bDepthMask = depthMask;
		newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));*/

		return newState;
	}

	long SmtD3DGPUStateManager::SetDepthTestState(SmtDepthTestState &state)
	{
		if (SMT_ERR_NONE == SetDepthTest(state.bEnabled) &&
			SMT_ERR_NONE == SetDepthTestFunc(state.cmpFunc))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtD3DGPUStateManager::SetDepthTest(bool enabled)
	{
		if (enabled == true)
		{
			//glEnable(GL_DEPTH_TEST);
		}
		else
		{
			//glDisable(GL_DEPTH_TEST);
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetDepthTestFunc(Comparison func)
	{
		//glDepthFunc(ConvertToGLEnum(COMPARISON_TABLE, func));

		return SMT_ERR_NONE;
	}

	SmtBlendState SmtD3DGPUStateManager::GetBlendState()
	{
		SmtBlendState newState;
		int sFactor = 0;
		int dFactor = 0;

		/*glGetIntegerv(GL_BLEND_SRC, &sFactor);
		glGetIntegerv(GL_BLEND_DST, &dFactor);

		newState.bEnabled = glIsEnabled(GL_BLEND);
		newState.srcFactor = static_cast<BlendFactor>(ConvertGLEnum(sFactor));
		newState.dstFactor = static_cast<BlendFactor>(ConvertGLEnum(dFactor));*/

		return newState;
	}

	long SmtD3DGPUStateManager::SetBlendState(SmtBlendState &state)
	{
		if (SMT_ERR_NONE != SetBlending(state.bEnabled))
			return SMT_ERR_FAILURE;
		 
		if (state.bEnabled)
		{
			/*uint sFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.srcFactor);
			uint dFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.dstFactor);
			glBlendFunc(sFactor, dFactor);*/
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetBlending(bool enabled)
	{
		if (enabled == true)
		{
			//glEnable(GL_BLEND);
		}
		else
		{
			//glDisable(GL_BLEND);
		}

		return SMT_ERR_NONE;
	}

	Viewport3D SmtD3DGPUStateManager::GetViewportState()
	{
		int values[4];

		//glGetIntegerv(GL_VIEWPORT, values);

		return Viewport3D(values[0], values[1], values[2], values[3],0,0,0);
	}

	long SmtD3DGPUStateManager::SetViewportState(Viewport3D &state)
	{
		//glViewport(state.ulX, state.ulY, state.ulWidth, state.ulHeight);

		return SMT_ERR_NONE;
	}

	SmtColor SmtD3DGPUStateManager::GetColorState()
	{
		float color[4];

		//glGetFloatv(GL_CURRENT_COLOR, color);

		return SmtColor(color[0], color[1], color[2], color[3]);
	}

	long SmtD3DGPUStateManager::SetColorState(SmtColor &colorState)
	{
		//glColor4f(colorState.fRed, colorState.fGreen, colorState.fBlue, colorState.fA);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::Set2DTextures(bool enabled)
	{
		if (enabled == true)
		{
			//glEnable(GL_TEXTURE_2D);
		}
		else
		{
			//glDisable(GL_TEXTURE_2D);
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::Set2DRectTextures(bool enabled)
	{
		if (enabled == true)
		{
			//glEnable(GL_TEXTURE_RECTANGLE_ARB);
		}
		else
		{
			//glDisable(GL_TEXTURE_RECTANGLE_ARB);
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetSampler(TextureSampler &sampler)
	{
		/* Check if anisotropy was set */
		/*if ((fabs(sampler.anisotropy - 0) < 1e-6))
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, sampler.anisotropy);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));*/

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetRectSampler(TextureSampler &sampler)
	{
		/* Check if anisotropy was set */
		/*if ((fabs(sampler.anisotropy - 0) < 1e-6))
		{
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAX_ANISOTROPY_EXT, sampler.anisotropy);
		}

		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_R, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));*/

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetTextureEnvironment(TextureEnvMode &envMode)
	{
		//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, ConvertToGLEnum(TEXTURE_ENVIRONMENT_TABLE, envMode.envMode));

		return SMT_ERR_NONE;
	}

	Matrix SmtD3DGPUStateManager::GetWorldViewMatrix()
	{
		Matrix result;
		
		//glGetFloatv(GL_MODELVIEW_MATRIX, (float *)&result);

		return result;
	}

	Matrix SmtD3DGPUStateManager::GetProjectionMatrix()
	{
		Matrix result;
	
		//glGetFloatv(GL_PROJECTION_MATRIX, (float *)&result);

		return result;
	}

	long SmtD3DGPUStateManager::SetWorldViewMatrix(Matrix &matrix)
	{
		//m_pD3DDevice->SetTransform( D3DTS_VIEW, (float*)(&matrix) );

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetProjectionMatrix(Matrix &matrix)
	{
		//m_pD3DDevice->SetTransform( D3DTS_PROJECTION, (float*)(&matrix) );

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::GetClearColorValue(float &red, float &green, float &blue, float &alpha)
	{
		float clr[4];
		//glGetFloatv(GL_CLEAR,clr);

		red = clr[0];
		green = clr[1];
		blue = clr[2];
		alpha = clr[3];

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetClearColorValue(float red, float green, float blue, float alpha)
	{
		//glClearColor(red, green, blue, alpha);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::GetClearDepthValue(float &depth)
	{
		//glGetFloatv(GL_DEPTH,&depth);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetClearDepthValue(float depth)
	{
		//glClearDepth(depth);

		return SMT_ERR_NONE;
	}
	
	long SmtD3DGPUStateManager::GetStencilClearValue(ulong &s)
	{
		int nS = 0;
		//glGetIntegerv(GL_STENCIL,&nS);
		s = nS;
		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetStencilClearValue( ulong s )
	{
		//glClearStencil(s);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetPolygonMode(FaceMode face, PolygonMode mode)
	{
		uint GLFace = ConvertToD3DEnum(FACE_TABLE, face);
		uint D3DMode = ConvertToD3DEnum(POLYGON_MODE_TABLE, mode);

		m_pD3DDevice->SetRenderState(D3DRS_FILLMODE,D3DMode);//ÉèÖÃÌî³ä×´Ì¬£¬µ±Ç°ÎªÃæÌî³ä×´Ì¬   */

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::GetLineWidth(float &size)
	{
		//glGetFloatv(GL_LINE_WIDTH,&size);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetLineWidth(float size)
	{
		//glLineWidth(size);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::GetPointSize(float &size)
	{
		//glGetFloatv(GL_POINT_SIZE,&size);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetPointSize(float size)
	{
		//glPointSize(size);

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetMaterail(bool enabled)
	{
		if (enabled)
		{
			//glEnable(GL_COLOR_MATERIAL);
		}
		else
			//glDisable(GL_COLOR_MATERIAL);
		
		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::SetLight(bool enabled)
	{
		if (enabled)
		{
			m_pD3DDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
		}
		else
			m_pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
		
		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::EnableDepthOffset(PolygonMode mode, bool enabled)
	{
		int tmp = 0;

		switch (mode)
		{
		case PM_POINT:
			//tmp = GL_POLYGON_OFFSET_POINT;
			break;
		case PM_LINE:
			//tmp = GL_POLYGON_OFFSET_LINE;
			break;
		case PM_FILL:
			//tmp = GL_POLYGON_OFFSET_FILL;
			break;
		default:
			;
		}

		if (enabled)
		{
			//glEnable(tmp);
		}
		else
		{
			//glDisable(tmp);
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DGPUStateManager::DepthOffsetParams(float rFactor, float dFactor)
	{
		//glPolygonOffset(rFactor, dFactor);
		
		return SMT_ERR_NONE;
	}
}

