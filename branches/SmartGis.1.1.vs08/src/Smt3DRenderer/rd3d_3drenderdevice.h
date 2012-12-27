/*
File:    rd3d_3drenderdevice.h

Desc:    Smt3DRenderDevice,ÈýÎ¬äÖÈ¾ÒýÇæ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_3DRENDERDEVICE_H
#define _RD3D_3DRENDERDEVICE_H

#include "rd3d_base.h"

#include "rd3d_3ddevicecaps.h"
#include "rd3d_indexbuffer.h"
#include "rd3d_vertexbuffer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_framebuffer.h"

#include "rd3d_statesmanager.h"
#include "rd3d_texturemanager.h"
#include "rd3d_shadermanager.h"
#include "rd3d_programmanager.h"

#include <vector>

namespace Smt_3Drd
{
	class Smt3DRenderDevice
	{
	public:
		Smt3DRenderDevice(void):m_hDLL(NULL)
			,m_rBaseApi(RA_OPENGL),m_strLogName("default")
			,m_bBlending(true){};
		virtual ~Smt3DRenderDevice(void){};

	public:
		virtual long			Init(HWND hWnd,const char* logname) = 0;
		virtual long			Destroy(void) = 0;
		virtual long			Release(void) = 0;

		inline	RenderBase3DApi GetBaseApi() const {return m_rBaseApi; }

	public:
		virtual SmtGPUStateManager* GetStateManager() = 0;
		virtual Smt3DDeviceCaps* GetDeviceCaps() = 0;

	public:
		//render
		virtual long			BeginRender() = 0;
		virtual long			EndRender() = 0;
		
		virtual long			DrawPrimitives(PrimitiveType type,SmtVertexBuffer* pVB,ulong baseVertex,ulong primitiveCount ) = 0;
		virtual long			DrawIndexedPrimitives(PrimitiveType type,SmtVertexBuffer* pVB,SmtIndexBuffer *pIB,ulong baseIndex,ulong primitiveCount) = 0;

		virtual long			DrawText(uint unID,float xpos,float ypos,float zpos, const SmtColor& color,const char*,...) = 0;
		virtual long			DrawText(uint unID,float xscreen,float yscreen, const SmtColor& color,const char*,...) = 0;

		virtual long			SwapBuffers() = 0;

		virtual long			SetClearColor(const SmtColor &clr) =0 ;
		virtual long			SetDepthClearValue(float z) = 0;
		virtual long			SetStencilClearValue(ulong s) = 0;
		virtual long			Clear(ulong flags) = 0;

		//matrix
		virtual long			MatrixModeSet(MatrixMode mode) = 0;
		virtual MatrixMode		MatrixModeGet() const = 0;
		virtual long			MatrixLoadIdentity() = 0;

		virtual long			MatrixLoad(const Matrix& m) = 0;
		virtual long			MatrixPush() = 0;
		virtual long			MatrixPop() = 0;
		virtual long			MatrixScale(float x,float y,float z) = 0;
		virtual long			MatrixTranslation(float x,float y,float z) = 0;
		virtual long			MatrixRotation(float angle,float x,float y,float z) = 0;
		virtual long			MatrixMultiply(const Matrix& m) = 0;
		virtual Matrix			MatrixGet() = 0;

		//get frustum
		virtual	long			GetFrustum(SmtFrustum &frustum) = 0;	

		//view
		virtual long			SetViewport(Viewport3D &viewport) = 0;
		virtual Viewport3D&		GetViewport(void) = 0;

		virtual long			SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar) = 0;
		virtual long			SetPerspective(float fovy,float aspect,float zNear,float zFar) = 0;

		//set viewlookat 
		virtual long			SetViewLookAt(Vector3 &vPos,Vector3 &vView,Vector3 &vUp) = 0;

		//2d to 3d
		virtual long			Transform2DTo3D(Vector3 &vOrg,Vector3 &vTar,const lPoint &point) = 0;

		//3d to 2d
		virtual long			Transform3DTo2D(const Vector3 &ver3D,lPoint &point) = 0;

	public:
		// aactivate additive blending
		virtual long			SetBlending(bool bBlending) = 0;

		// activate backface culling
		virtual long			SetBackfaceCulling(RenderStateValue rsv ) = 0;

		// activate stencil buffer
		virtual long			SetStencilBufferMode(RenderStateValue rsv,ulong ul)=0;

		// activate depth buffer
		virtual long			SetDepthBufferMode(RenderStateValue rsv)=0;
	
		// activate wireframe mode
		virtual long			SetShadeMode(RenderStateValue rsv,float v,const SmtColor &clr)=0;
	public:
		//light stuff
		virtual long			SetLight(int index,SmtLight *pLight)=0;
		virtual long			SetAmbientLight(const SmtColor &clr)=0;

		//set texture
		virtual long			SetTexture(SmtTexture *pTex) = 0;

		//material stuff
		virtual long			SetMaterial(SmtMaterial *pMat) =0;

		//fog stuff
		virtual long			SetFog(FogMode mode, const SmtColor& colour, float density, float start, float end) = 0;
		
	public:
		//////////////////////////////////////////////////////////////////////////vbuf
		//vertex buffer
		virtual SmtVertexBuffer	*CreateVertexBuffer(int nCount,ulong unFormat,bool bDynamic = true) = 0;

		//index buffer
		virtual SmtIndexBuffer	*CreateIndexBuffer(int nCount) = 0;

		//video buffer
		virtual SmtVideoBuffer* CreateVideoBuffer(ArrayType type) = 0;
		virtual long			DestroyBuffer(SmtVideoBuffer *buffer) = 0;
		virtual long			DestroyIndexBuffer(SmtVideoBuffer *buffer) = 0;
		
		//vba
		virtual long			SetVertexArray(int components, Type type, int stride, void* data) = 0;
		virtual long			SetTextureCoordsArray(int components, Type type, int stride, void* data) = 0;
		virtual long			SetNormalArray(Type type, int stride, void* data) = 0;
		virtual long			SetIndexArray(Type type, int stride, void* data) = 0;
		virtual long			EnableArray(ArrayType type, bool enabled) = 0;

		//vbo
		virtual long			BindBuffer(SmtVideoBuffer *buffer) = 0;
		virtual long			BindIndexBuffer(SmtVideoBuffer *buffer) = 0;
		virtual long			UnbindBuffer() = 0;
		virtual long			UnbindIndexBuffer() = 0;
		virtual long			UpdateBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method) = 0;
		virtual long			UpdateIndexBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method) = 0;
		virtual void			*MapBuffer(SmtVideoBuffer* buffer, AccessMode access) = 0;
		virtual long			UnmapBuffer(SmtVideoBuffer *buffer) = 0;
		virtual void			*MapIndexBuffer(SmtVideoBuffer* buffer, AccessMode access) = 0;
		virtual long			UnmapIndexBuffer(SmtVideoBuffer *buffer) = 0;
		
		//////////////////////////////////////////////////////////////////////////shader
		//vertex shader
		virtual SmtShader		*CreateVertexShader(const char *szName) = 0;
		
		//pixel shader
		virtual SmtShader		*CreatePixelShader(const char *szName) = 0;

		virtual SmtShader		*GetShader(const char *szName) = 0;
		virtual long			DestroyShader(const char *szName) = 0;

		//program
		virtual SmtProgram		*CreateProgram(const char *szName) = 0;
		virtual SmtProgram		*GetProgram(const char *szName) = 0;
		virtual long			DestroyProgram(const char *szName) = 0;

		virtual long			LoadShaderSource(SmtShader *shader, char* source) = 0;
		virtual long			CompileShader(SmtShader *shader) = 0;
		virtual long			IsShaderCompiled(SmtShader *shader) = 0;
		virtual char			*GetShaderLog(SmtShader *shader) = 0;
		
		virtual long			BindProgram(SmtProgram *program) = 0;
		virtual long			UnbindProgram() = 0;
		virtual long			SetProgramVertexShader(SmtProgram *program, SmtShader *shader) = 0;
		virtual long			SetProgramPixelShader(SmtProgram *program, SmtShader *shader) = 0;
		virtual long			LinkProgram(SmtProgram *program) = 0;
		virtual long			IsProgramLinked(SmtProgram *program) = 0;
		virtual char			*GetProgramLinkLog(SmtProgram *program) = 0;
	
		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector4 &value) = 0;
		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector3 &value) = 0;
		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector2 &value) = 0;
		virtual long			SetProgramFloat(SmtProgram *program, string &param, float value) = 0;
		virtual long			SetProgramInt(SmtProgram *program, string &param, int value) = 0;
		virtual long			GetProgramFloat(SmtProgram *program, string &param, float *value) = 0;

		virtual long			SetProgramTexture(SmtProgram *program, string &param, int texture) = 0;

		//////////////////////////////////////////////////////////////////////////texture
		//texture
		virtual	SmtTexture		*CreateTexture(const char *szName) = 0;
		virtual	SmtTexture		*GetTexture(const char *szName) = 0;
		virtual long			DestroyTexture(const char *szName) = 0;

		virtual	long			GenerateMipmap(SmtTexture *texture) = 0;
		virtual long			BindTexture(SmtTexture *texture) = 0;
		virtual long			BuildTexture(SmtTexture *texture) = 0;
		virtual long			BindRectTexture(SmtTexture *texture) = 0;
		virtual long			UnbindTexture() = 0;
		virtual long			UnbindRectTexture() = 0;
	
		//////////////////////////////////////////////////////////////////////////
		//frame buffer
		virtual SmtFrameBuffer* CreateFrameBuffer() = 0;
		virtual long			DestroyFrameBuffer(SmtFrameBuffer *frameBuffer) = 0;
		virtual long			BindFrameBuffer(SmtFrameBuffer *frameBuffer) = 0;
		virtual long			UnbindFrameBuffer() = 0;

		virtual SmtRenderBuffer* CreateRenderBuffer(TextureFormat format, uint width, uint height) = 0;
		virtual long			DestroyRenderBuffer(SmtRenderBuffer *renderBuffer) = 0;
		virtual long			AttachRenderBuffer(SmtFrameBuffer *frameBuffer, SmtRenderBuffer *renderBuffer, RenderBufferSlot slot) = 0;
		virtual long			AttachTexture(SmtFrameBuffer *frameBuffer, SmtTexture *texture2D, RenderBufferSlot slot) = 0;
		virtual FrameBufferStatus CheckFrameBufferStatus() = 0;

		//////////////////////////////////////////////////////////////////////////font
		//font
		virtual long			CreateFont(const char *chType,int nHeight,int nWidth,int nWeight,bool bItalic,bool bUnderline,bool bStrike,ulong unSize,uint &unID) = 0;		

	public:
		//fast draw
		virtual long			DrawCube3D(Vector3 vCenter,float fWidth,SmtColor smtClr) = 0;
	
	protected:
		HINSTANCE				m_hDLL;
		RenderBase3DApi			m_rBaseApi;
		MatrixMode				m_matrixMode;
		Viewport3D				m_viewPort;
		string					m_strLogName;

	protected:
		SmtShaderManager		m_shaderMgr;
		SmtProgramManager		m_progamMgr;
		SmtTextureManager		m_textureMgr;

	protected:
		bool					m_bBlending;

	};

	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	extern "C" 
	{
		_declspec(dllexport) HRESULT Create3DRenderDevice(HINSTANCE hDLL, Smt3DRenderDevice* & pInterface);
		typedef HRESULT (*_Create3DRenderDevice)(HINSTANCE hDLL, Smt3DRenderDevice* &pInterface);

		_declspec(dllexport) HRESULT Release3DRenderDevice(Smt3DRenderDevice* & pInterface);
		typedef HRESULT (*_Release3DRenderDevice)(Smt3DRenderDevice* & pInterface);
	}
}

#endif //_RD3D_3DRENDERDEVICE_H

