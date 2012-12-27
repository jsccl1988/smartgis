
/*
File:    gl_statesmanager.h

Desc:    SmtGLGPUStateManager

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GL_STATESMANAGER_H
#define _GL_STATESMANAGER_H

#include "gl_prerequisites.h"
#include "rd3d_statesmanager.h"
#include <map>

namespace Smt_3Drd
{
	class SmtGLGPUStateManager:public SmtGPUStateManager
	{
	public:
		SmtGLGPUStateManager();

	public:
		//alpha test
		virtual SmtAlphaTestState	GetAlphaTestState(void);
		virtual long				SetAlphaTestState(SmtAlphaTestState& state);
		virtual long				SetAlphaTest(bool enabled);
		virtual long				SetAlphaTestFunc(Comparison func, float ref);

		//depth test
		virtual SmtDepthTestState	GetDepthTestState(void);
		virtual long				SetDepthTestState(SmtDepthTestState& state);
		virtual long				SetDepthTest(bool enabled);
		virtual long				SetDepthTestFunc(Comparison func);

		//blending
		virtual SmtBlendState		GetBlendState(void);
		virtual long				SetBlendState(SmtBlendState &state);
		virtual long				SetBlending(bool enabled);

		//viewport
		virtual Viewport3D			GetViewportState(void);
		virtual long				SetViewportState(Viewport3D &vp);
	
		//clr
		virtual SmtColor			GetColorState(void);
		virtual long				SetColorState(SmtColor &colorState);
		
		//texture
		virtual long				Set2DTextures(bool enabled);
		virtual long				Set2DRectTextures(bool enabled);

		virtual long				SetSampler(TextureSampler &sampler);
		virtual long				SetRectSampler(TextureSampler &sampler);
		virtual long				SetTextureEnvironment(TextureEnvMode &envMode);

		//matrix 
		virtual Matrix				GetWorldViewMatrix(void);
		virtual Matrix				GetProjectionMatrix(void);

		virtual long				SetWorldViewMatrix(Matrix& matrix);
		virtual long				SetProjectionMatrix(Matrix& matrix);

		//util
		virtual long				GetClearColorValue(float &red, float &green, float &blue, float &alpha);
		virtual long				SetClearColorValue(float red, float green, float blue, float alpha = 1.f);
		virtual long				GetClearDepthValue(float &depth);	
		virtual long				SetClearDepthValue(float depth);
		virtual	long				GetStencilClearValue( ulong &s );
		virtual	long				SetStencilClearValue( ulong s );

		virtual long				SetPolygonMode(FaceMode face, PolygonMode mode);

		virtual long				GetLineWidth(float &size);
		virtual long				SetLineWidth(float size);
		virtual long				GetPointSize(float &size);
		virtual long				SetPointSize(float size);

		virtual long				EnableDepthOffset(PolygonMode mode, bool enabled);
		virtual long				DepthOffsetParams(float rFactor, float dFactor);

		virtual long				SetMaterail(bool enabled);
		virtual long				SetLight(bool enabled);

	private:
		int							ConvertGLEnum(GLenum value);
		GLenum						ConvertToGLEnum(uint tableIndex, uint value);

	private:
		map<int, int>				m_GLToDev;
	};
}

#endif //_GL_STATESMANAGER_H