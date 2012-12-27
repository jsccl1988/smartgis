#include "gl_statesmanager.h"

namespace Smt_3Drd
{
	/**
	Map to convert OpenGL consts to engine's and vice versa
	*/
	const static int g_GLToEngineMap[][2] =
	{
		{GL_LESS, CMP_LESS}, {GL_LEQUAL, CMP_LEQUAL}, {GL_GREATER, CMP_GREATER}, {GL_GEQUAL, CMP_GEQUAL}, {GL_EQUAL, CMP_EQUAL},
		{GL_ZERO, BF_ZERO}, {GL_ONE, BF_ONE}, {GL_SRC_COLOR, BF_SRC_COLOR}, {GL_DST_COLOR, BF_DST_COLOR},
		{GL_ONE_MINUS_DST_COLOR, BF_ONE_MINUS_DST_COLOR}, {GL_SRC_ALPHA, BF_SRC_ALPHA},
		{GL_ONE_MINUS_SRC_ALPHA, BF_ONE_MINUS_SRC_ALPHA}, {GL_DST_ALPHA, BF_DST_ALPHA},
		{GL_ONE_MINUS_DST_ALPHA, BF_ONE_MINUS_DST_ALPHA}, {GL_SRC_ALPHA_SATURATE, BF_SRC_ALPHA_SATURATE}
	};

	/* Indexes of map tables for different constants */
	const static int COMPARISON_TABLE              = 0;
	const static int TEXTURE_FILTER_TABLE          = 1;
	const static int TEXTURE_COORD_WRAP_MODE_TABLE = 2;
	const static int TEXTURE_ENVIRONMENT_TABLE     = 3;
	const static int FACE_TABLE                    = 4;
	const static int POLYGON_MODE_TABLE            = 5;
	const static int BLEND_FACTOR_TABLE            = 6;

	const static int g_ComparisonTable[] = { GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL, GL_EQUAL };

	const static int g_TextureFilterTable[] =
	{ GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR };

	const static int g_TextureCoordWrapModeTable[] = { GL_CLAMP, GL_REPEAT, GL_CLAMP_TO_EDGE };

	const static int g_TextureEnvironmentTable[] = { GL_REPLACE, GL_MODULATE, GL_DECAL, GL_BLEND, GL_ADD };

	const static int g_FaceTable[] = { GL_BACK, GL_FRONT, GL_FRONT_AND_BACK };

	const static int g_PolygonModeTable[] = { GL_POINT, GL_LINE, GL_FILL };

	const static int g_BlendFactorTable[] =
	{
		GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE
	};

	const static int* g_EngineToGLMap[] =
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

	const static int g_TablesCount = sizeof(g_EngineToGLMap) / sizeof(int*);

	/**
	Default constructor.
	*/
	SmtGLGPUStateManager::SmtGLGPUStateManager()
	{
		for (int i = 0; i < g_TablesCount; i++)
		{
			m_GLToDev[g_GLToEngineMap[i][0]] = g_GLToEngineMap[i][1];
		}
	}

	int SmtGLGPUStateManager::ConvertGLEnum(GLenum value)
	{
		return m_GLToDev[(int)value];
	}

	GLenum SmtGLGPUStateManager::ConvertToGLEnum(uint tableIndex, uint value)
	{
		if (tableIndex < g_TablesCount &&
			value < g_TableSize[tableIndex])
		{
			return g_EngineToGLMap[tableIndex][value];
		}

		return -1;
	}

	SmtAlphaTestState SmtGLGPUStateManager::GetAlphaTestState()
	{
		SmtAlphaTestState newState;
		int func = 0;
		float ref = 0;

		glGetIntegerv(GL_ALPHA_TEST_FUNC, &func);
		glGetFloatv(GL_ALPHA_TEST_REF, &ref);

		newState.bEnabled = glIsEnabled(GL_ALPHA_TEST);
		newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));
		newState.fRefValue = ref;

		return newState;
	}

	long SmtGLGPUStateManager::SetAlphaTestState(SmtAlphaTestState &state)
	{
		if (SMT_ERR_NONE == SetAlphaTest(state.bEnabled) &&
			SMT_ERR_NONE == SetAlphaTestFunc(state.cmpFunc, state.fRefValue))
		{
			return SMT_ERR_NONE;
		}
		
		return SMT_ERR_FAILURE;
	}

	long SmtGLGPUStateManager::SetAlphaTest(bool enabled)
	{
		if (enabled == true)
		{
			glEnable(GL_ALPHA_TEST);
		}
		else
		{
			glDisable(GL_ALPHA_TEST);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetAlphaTestFunc(Comparison func, float ref)
	{
		glAlphaFunc(ConvertToGLEnum(COMPARISON_TABLE, func), ref);

		return SMT_ERR_NONE;
	}

	SmtDepthTestState SmtGLGPUStateManager::GetDepthTestState()
	{
		SmtDepthTestState newState;
		GLboolean depthMask;
		int func = 0;

		glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
		glGetIntegerv(GL_DEPTH_FUNC, &func);

		newState.bEnabled = glIsEnabled(GL_DEPTH_TEST);
		newState.bDepthMask = depthMask;
		newState.cmpFunc = static_cast<Comparison>(ConvertGLEnum(func));

		return newState;
	}

	long SmtGLGPUStateManager::SetDepthTestState(SmtDepthTestState &state)
	{
		if (SMT_ERR_NONE == SetDepthTest(state.bEnabled) &&
			SMT_ERR_NONE == SetDepthTestFunc(state.cmpFunc))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtGLGPUStateManager::SetDepthTest(bool enabled)
	{
		if (enabled == true)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetDepthTestFunc(Comparison func)
	{
		glDepthFunc(ConvertToGLEnum(COMPARISON_TABLE, func));

		return SMT_ERR_NONE;
	}

	SmtBlendState SmtGLGPUStateManager::GetBlendState()
	{
		SmtBlendState newState;
		int sFactor = 0;
		int dFactor = 0;

		glGetIntegerv(GL_BLEND_SRC, &sFactor);
		glGetIntegerv(GL_BLEND_DST, &dFactor);

		newState.bEnabled = glIsEnabled(GL_BLEND);
		newState.srcFactor = static_cast<BlendFactor>(ConvertGLEnum(sFactor));
		newState.dstFactor = static_cast<BlendFactor>(ConvertGLEnum(dFactor));

		return newState;
	}

	long SmtGLGPUStateManager::SetBlendState(SmtBlendState &state)
	{
		if (SMT_ERR_NONE != SetBlending(state.bEnabled))
			return SMT_ERR_FAILURE;
		 
		if (state.bEnabled)
		{
			GLenum sFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.srcFactor);
			GLenum dFactor = ConvertToGLEnum(BLEND_FACTOR_TABLE, state.dstFactor);
			glBlendFunc(sFactor, dFactor);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetBlending(bool enabled)
	{
		if (enabled == true)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}
		return SMT_ERR_NONE;
	}

	Viewport3D SmtGLGPUStateManager::GetViewportState()
	{
		int values[4];

		glGetIntegerv(GL_VIEWPORT, values);

		return Viewport3D(values[0], values[1], values[2], values[3],0,0,0);
	}

	long SmtGLGPUStateManager::SetViewportState(Viewport3D &state)
	{
		glViewport(state.ulX, state.ulY, state.ulWidth, state.ulHeight);

		return SMT_ERR_NONE;
	}

	SmtColor SmtGLGPUStateManager::GetColorState()
	{
		float color[4];

		glGetFloatv(GL_CURRENT_COLOR, color);

		return SmtColor(color[0], color[1], color[2], color[3]);
	}

	long SmtGLGPUStateManager::SetColorState(SmtColor &colorState)
	{
		glColor4f(colorState.fRed, colorState.fGreen, colorState.fBlue, colorState.fA);

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::Set2DTextures(bool enabled)
	{
		if (enabled == true)
		{
			glEnable(GL_TEXTURE_2D);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::Set2DRectTextures(bool enabled)
	{
		if (enabled == true)
		{
			glEnable(GL_TEXTURE_RECTANGLE_ARB);
		}
		else
		{
			glDisable(GL_TEXTURE_RECTANGLE_ARB);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetSampler(TextureSampler &sampler)
	{
		/* Check if anisotropy was set */
		if ((fabs(sampler.anisotropy - 0) < 1e-6))
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, sampler.anisotropy);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetRectSampler(TextureSampler &sampler)
	{
		/* Check if anisotropy was set */
		if ((fabs(sampler.anisotropy - 0) < 1e-6))
		{
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAX_ANISOTROPY_EXT, sampler.anisotropy);
		}

		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.magFilter));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, ConvertToGLEnum(TEXTURE_FILTER_TABLE, sampler.minFilter));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.sTexture));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.tTexture));
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_R, ConvertToGLEnum(TEXTURE_COORD_WRAP_MODE_TABLE, sampler.rTexture));

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetTextureEnvironment(TextureEnvMode &envMode)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, ConvertToGLEnum(TEXTURE_ENVIRONMENT_TABLE, envMode.envMode));

		return SMT_ERR_NONE;
	}

	Matrix SmtGLGPUStateManager::GetWorldViewMatrix()
	{
		Matrix result;
		
		glGetFloatv(GL_MODELVIEW_MATRIX, (float *)&result);

		return result;
	}

	Matrix SmtGLGPUStateManager::GetProjectionMatrix()
	{
		Matrix result;
	
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)&result);

		return result;
	}

	long SmtGLGPUStateManager::SetWorldViewMatrix(Matrix &matrix)
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((float*)(&matrix));

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetProjectionMatrix(Matrix &matrix)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((float*)(&matrix));

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::GetClearColorValue(float &red, float &green, float &blue, float &alpha)
	{
		GLfloat clr[4];
		glGetFloatv(GL_CLEAR,clr);
		red = clr[0];
		green = clr[1];
		blue = clr[2];
		alpha = clr[3];

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetClearColorValue(float red, float green, float blue, float alpha)
	{
		glClearColor(red, green, blue, alpha);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::GetClearDepthValue(float &depth)
	{
		glGetFloatv(GL_DEPTH,&depth);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetClearDepthValue(float depth)
	{
		glClearDepth(depth);
		return SMT_ERR_NONE;
	}
	
	long SmtGLGPUStateManager::GetStencilClearValue(ulong &s)
	{
		int nS = 0;
		glGetIntegerv(GL_STENCIL,&nS);
		s = nS;
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetStencilClearValue( ulong s )
	{
		glClearStencil(s);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetPolygonMode(FaceMode face, PolygonMode mode)
	{
		GLenum GLFace = ConvertToGLEnum(FACE_TABLE, face);
		GLenum GLMode = ConvertToGLEnum(POLYGON_MODE_TABLE, mode);

		glPolygonMode(GLFace, GLMode);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::GetLineWidth(float &size)
	{
		glGetFloatv(GL_LINE_WIDTH,&size);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetLineWidth(float size)
	{
		glLineWidth(size);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::GetPointSize(float &size)
	{
		glGetFloatv(GL_POINT_SIZE,&size);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetPointSize(float size)
	{
		glPointSize(size);
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetMaterail(bool enabled)
	{
		if (enabled)
		{
			glEnable(GL_COLOR_MATERIAL);
		}
		else
			glDisable(GL_COLOR_MATERIAL);
		
		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::SetLight(bool enabled)
	{
		if (enabled)
		{
			glEnable(GL_LIGHTING);
		}
		else
			glDisable(GL_LIGHTING);

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::EnableDepthOffset(PolygonMode mode, bool enabled)
	{
		int tmp = 0;

		switch (mode)
		{
		case PM_POINT:
			tmp = GL_POLYGON_OFFSET_POINT;
			break;
		case PM_LINE:
			tmp = GL_POLYGON_OFFSET_LINE;
			break;
		case PM_FILL:
			tmp = GL_POLYGON_OFFSET_FILL;
			break;
		default:
			;
		}

		if (enabled)
		{
			glEnable(tmp);
		}
		else
		{
			glDisable(tmp);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLGPUStateManager::DepthOffsetParams(float rFactor, float dFactor)
	{
		glPolygonOffset(rFactor, dFactor);
		
		return SMT_ERR_NONE;
	}
}

