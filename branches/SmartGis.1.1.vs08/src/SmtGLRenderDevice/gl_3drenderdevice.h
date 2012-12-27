/*
File:    gl_3drenderdevice.h

Desc:    SmtGLRenderDevice,opengl ÈýÎ¬äÖÈ¾Çý¶¯

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GL_3DRENDERDEVICE_H
#define _GL_3DRENDERDEVICE_H

#include "rd3d_3drenderdevice.h"
#include "gl_prerequisites.h"
#include "gl_statesmanager.h"

#include "gl_shaderfunc.h"
#include "gl_multitexturefunc.h"
#include "gl_vsyncfunc.h"
#include "gl_mipmapfunc.h"
#include "gl_vbofunc.h"
#include "gl_fbofunc.h"

#include "gl_devicecaps.h"
//
namespace Smt_3Drd
{
	#define MAX_LIGHTS 8

	class SmtGLText;

	class SmtGLRenderDevice:public Smt3DRenderDevice
	{
	public:
		SmtGLRenderDevice(void);
		SmtGLRenderDevice(HINSTANCE hDLL);
		virtual ~SmtGLRenderDevice(void);

	public:
		virtual long			Init(HWND hWnd,const char* logname);   //initial 
		virtual long			Release(void);                         //release ptr or close file
		virtual long			Destroy(void);                         //destroy gl object

		inline	RenderBase3DApi GetBaseApi() const {return m_rBaseApi; }

		bool					IsExtensionSupported(string extension);
		void*					GetProcAddress(string name);

		virtual SmtGPUStateManager* GetStateManager();
		virtual Smt3DDeviceCaps* GetDeviceCaps();

	public:
		//render
		virtual long			BeginRender();
		virtual long			EndRender();
	
		virtual long			DrawPrimitives(PrimitiveType type,SmtVertexBuffer * pVB,ulong baseVertex,ulong primitiveCount );
		virtual long			DrawIndexedPrimitives(PrimitiveType type,SmtVertexBuffer * pVB,SmtIndexBuffer *pIB,ulong baseIndex,ulong primitiveCount);

		virtual long			DrawText(uint unID,float xpos,float ypos,float zpos, const SmtColor& color,const char*,...);
		virtual long			DrawText(uint nID,float xscreen,float yscreen, const SmtColor& color,const char*,...);

		virtual long			SwapBuffers();

		virtual long			SetClearColor(const SmtColor &clr);
		virtual long			SetDepthClearValue(float z);
		virtual long			SetStencilClearValue(ulong s);
		virtual long			Clear(ulong flags);

		//matrix
		virtual long			MatrixModeSet(MatrixMode mode);
		virtual MatrixMode		MatrixModeGet() const;
		virtual long			MatrixLoadIdentity();

		virtual long			MatrixLoad(const Matrix& m);
		virtual long			MatrixPush();
		virtual long			MatrixPop();
		virtual long			MatrixScale(float x,float y,float z);
		virtual long			MatrixTranslation(float x,float y,float z);
		virtual long			MatrixRotation(float angle,float x,float y,float z);
		virtual long			MatrixMultiply(const Matrix& m);
		virtual Matrix			MatrixGet();

		//get frustum
		virtual	long			GetFrustum(SmtFrustum &frustum);	

		//view
		virtual long			SetViewport(Viewport3D &viewport);
		virtual Viewport3D&		GetViewport(void) {return m_viewPort;}

		virtual long			SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar);
		virtual long			SetPerspective(float fovy,float aspect,float zNear,float zFar);

		//set viewlookat 
		virtual long			SetViewLookAt(Vector3 &vPos,Vector3 &vView,Vector3 &vUp);

		//2d to 3d
		virtual long			Transform2DTo3D(Vector3 &vOrg,Vector3 &vTar,const lPoint &point);

		//3d to 2d
		virtual long			Transform3DTo2D(const Vector3 &ver3D,lPoint &point);

	public:
		// aactivate additive blending
		virtual long			SetBlending(bool bBlending);

		// activate backface culling
		virtual long			SetBackfaceCulling(RenderStateValue rsv);

		// activate stencil buffer
		virtual long			SetStencilBufferMode(RenderStateValue rsv,ulong ul);

		// activate depth buffer
		virtual long			SetDepthBufferMode(RenderStateValue rsv);

		// activate wireframe mode
		virtual long			SetShadeMode(RenderStateValue rsv,float,const SmtColor &clr);
	public:
		//light stuff
		virtual long			SetLight(int index,SmtLight *pLight);
		virtual long			SetAmbientLight(const SmtColor &clr);

		//set texture
		virtual long			SetTexture(SmtTexture *pTex);

		//material stuff
		virtual long			SetMaterial(SmtMaterial *pMat);

		//fog stuff
		virtual long			SetFog(FogMode mode, const SmtColor& color, float density, float start, float end);

	public:
		//////////////////////////////////////////////////////////////////////////
		//vertex buffer
		virtual SmtVertexBuffer	*CreateVertexBuffer(int nCount,ulong ulFormat,bool bDynamic = true);

		//index buffer
		virtual SmtIndexBuffer	*CreateIndexBuffer(int nCount);

		//video buffer
		virtual SmtVideoBuffer* CreateVideoBuffer(ArrayType type);
		virtual long			DestroyBuffer(SmtVideoBuffer *buffer);
		virtual long			DestroyIndexBuffer(SmtVideoBuffer *buffer);

		//vba
		virtual long			SetVertexArray(int components, Type type, int stride, void* data);
		virtual long			SetTextureCoordsArray(int components, Type type, int stride, void* data);
		virtual long			SetNormalArray(Type type, int stride, void* data);
		virtual long			SetIndexArray(Type type, int stride, void* data);
		virtual long			EnableArray(ArrayType type, bool enabled);

		//vbo
		virtual long			BindBuffer(SmtVideoBuffer *buffer);
		virtual long			BindIndexBuffer(SmtVideoBuffer *buffer);
		virtual long			UnbindBuffer();
		virtual long			UnbindIndexBuffer();
		virtual long			UpdateBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method);
		virtual long			UpdateIndexBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method);
		virtual void			*MapBuffer(SmtVideoBuffer* buffer, AccessMode access);
		virtual long			UnmapBuffer(SmtVideoBuffer *buffer);
		virtual void			*MapIndexBuffer(SmtVideoBuffer* buffer, AccessMode access);
		virtual long			UnmapIndexBuffer(SmtVideoBuffer *buffer);

		
		//////////////////////////////////////////////////////////////////////////
		//shader
		virtual SmtShader		*CreateVertexShader(const char *szName);	//vertex shader
		virtual SmtShader		*CreatePixelShader(const char *szName);		//pixel shader
		virtual long			DestroyShader(const char *szName);
		virtual SmtShader		*GetShader(const char *szName);

		//program
		virtual SmtProgram		*CreateProgram(const char *szName);
		virtual long			DestroyProgram(const char *szName);
		virtual SmtProgram		*GetProgram(const char *szName);


		virtual long			LoadShaderSource(SmtShader *shader, char* source);
		virtual long			CompileShader(SmtShader *shader);
		virtual long			IsShaderCompiled(SmtShader *shader);
		virtual char			*GetShaderLog(SmtShader *shader);
		
		virtual long			BindProgram(SmtProgram *program);
		virtual long			UnbindProgram();
		virtual long			SetProgramVertexShader(SmtProgram *program, SmtShader *shader);
		virtual long			SetProgramPixelShader(SmtProgram *program, SmtShader *shader);
		virtual long			LinkProgram(SmtProgram *program);
		virtual long			IsProgramLinked(SmtProgram *program);
		virtual char			*GetProgramLinkLog(SmtProgram *program);
		

		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector4 &value);
		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector3 &value);
		virtual long			SetProgramVector(SmtProgram *program, string &param, const Vector2 &value);
		virtual long			SetProgramFloat(SmtProgram *program, string &param, float value);
		virtual long			SetProgramInt(SmtProgram *program, string &param, int value);
		virtual long			GetProgramFloat(SmtProgram *program, string &param, float *value);

		virtual long			SetProgramTexture(SmtProgram *program, string &param, int texture);

		//////////////////////////////////////////////////////////////////////////
		//texture
		virtual	SmtTexture		*CreateTexture(const char *szName);
		virtual long			DestroyTexture(const char *szName);
		virtual	SmtTexture		*GetTexture(const char *szName);

		virtual	long			GenerateMipmap(SmtTexture *texture);
		virtual long			BindTexture(SmtTexture *texture);
		virtual long			BuildTexture(SmtTexture *texture);
		virtual long			BindRectTexture(SmtTexture *texture);
		virtual long			UnbindTexture();
		virtual long			UnbindRectTexture(void);
	
		//////////////////////////////////////////////////////////////////////////
		virtual SmtFrameBuffer* CreateFrameBuffer();
		virtual long			DestroyFrameBuffer(SmtFrameBuffer *frameBuffer);

		virtual long			BindFrameBuffer(SmtFrameBuffer *frameBuffer);
		virtual long			UnbindFrameBuffer();
		virtual SmtRenderBuffer* CreateRenderBuffer(TextureFormat format, uint width, uint height);
		virtual long			DestroyRenderBuffer(SmtRenderBuffer *renderBuffer);

		virtual long			AttachRenderBuffer(SmtFrameBuffer *frameBuffer, SmtRenderBuffer *renderBuffer, RenderBufferSlot slot);
		virtual long			AttachTexture(SmtFrameBuffer *frameBuffer, SmtTexture *texture2D, RenderBufferSlot slot);
		virtual FrameBufferStatus CheckFrameBufferStatus();

		//ceate font
		virtual long			CreateFont(const char *szChType,int nHeight,int nWidth,int nWeight,bool bItalic,bool bUnderline,bool bStrike,ulong dwSize,uint &unID );		

	public:
		//fast draw
		virtual long			DrawCube3D(Vector3 vCenter,float fWidth,SmtColor smtClr);

	protected:
		long					SetDeviceCaps(void);
		inline long				GetOpenGLPrimitiveType( const PrimitiveType pt,const ulong nInitialPrimitiveCount,GLenum* GLPrimitiveType,ulong* nGLPrimitiveCount );
		
		GLenum 					ConvertRenderBufferSlot(RenderBufferSlot slot);
		GLenum					ConvertType(Type type);
		GLenum					ConvertVideoBufferStoreMethod(VideoBufferStoreMethod method);
		GLenum					ConvertAccess(AccessMode access);
		GLenum					ConvertArrayType(ArrayType type);
		GLenum					ConvertTexFormat(TextureFormat format);

	protected:
		vector<GLhandleARB>		m_vGLHandles;

		SmtGLGPUStateManager	*m_pStateManager;
		SmtGLDeviceCaps			*m_pDeviceCaps;

		//ext func
		SmtShadersFunc			*m_pFuncShaders;
		SmtMultitextureFunc		*m_pFuncMultTex;
		SmtVSyncFunc			*m_pFuncVSync;
		SmtMipmapFunc			*m_pFuncMipmap;
		SmtVBOFunc				*m_pFuncVBO;
		SmtFBOFunc				*m_pFuncFBO;

	protected:
		HWND					m_hWnd;
		HGLRC					m_hRC;
		vector<SmtGLText *>		m_vTextPtrs;

	private:
		int						m_nStencilRef;
		uint					m_unStencilCmp;				//stencil cmp function
		uint					m_unStencilWriteMask;		//WriteMask
		uint					m_unStencilMask;			//mask
		uint					m_unOpStencilFail;			//stencil fail op
		uint					m_unOpStencilZPass;			//stencil pass
		uint					m_unOpStencilZFail;			//stencil pass but zbuf fail op
	};
}

#endif //_GL_3DRENDERDEVICE_H
