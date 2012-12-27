
/*
File:    d3d_statesmanager.h

Desc:    SmtD3DGPUStateManager

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3D_STATESMANAGER_H
#define _D3D_STATESMANAGER_H

#include "d3d_prerequisites.h"
#include "rd3d_statesmanager.h"
#include <map>

namespace Smt_3Drd
{
	class SmtD3DGPUStateManager:public SmtGPUStateManager
	{
	public:
		SmtD3DGPUStateManager(IDirect3DDevice9		*pD3DDevice);

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
		int							ConvertD3DEnum(uint value);
		uint						ConvertToD3DEnum(uint tableIndex, uint value);

	private:
		map<int, int>				m_D3DToDev;

		IDirect3DDevice9			*m_pD3DDevice;
	};
}

#endif //_D3D_STATESMANAGER_H