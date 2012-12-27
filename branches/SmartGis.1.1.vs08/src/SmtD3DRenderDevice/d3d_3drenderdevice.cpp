#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"
#include "d3d_multitexturefuncimp.h"
#include "d3d_shaderfuncimp.h"
#include "d3d_vsyncfuncimp.h"
#include "d3d_mipmapfuncimp.h"
#include "d3d_vbofuncimp.h"
#include "d3d_fbofuncimp.h"
#include "d3d_text.h"
#include "rd3d_texturemanager.h"
#include "rd3d_programmanager.h"
#include "rd3d_shadermanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	SmtD3DRenderDevice::SmtD3DRenderDevice():m_pDeviceCaps(NULL)
		,m_pStateManager(NULL)
		,m_pFuncShaders(NULL)
		,m_pFuncMultTex(NULL)
		,m_pFuncVSync(NULL)
		,m_pFuncMipmap(NULL)
	{
		m_rBaseApi = RA_D3D09; 
		m_hDLL = NULL;
		m_strLogName = "";

		
	}

	SmtD3DRenderDevice::SmtD3DRenderDevice(HINSTANCE hDLL):m_pDeviceCaps(NULL)
		,m_pStateManager(NULL)
		,m_pFuncShaders(NULL)
		,m_pFuncMultTex(NULL)
		,m_pFuncVSync(NULL)
		,m_pFuncMipmap(NULL)
	{
	   m_rBaseApi = RA_D3D09; 
	   m_hDLL = hDLL;
	   m_strLogName = "";
	 
	}

	SmtD3DRenderDevice::~SmtD3DRenderDevice()
	{
		Release();
	}

	long SmtD3DRenderDevice::Init(HWND hWnd,const char* logname)
	{
		assert(::IsWindow(hWnd));
		m_hWnd = hWnd;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->CreateLog(logname);

		if (NULL == pLog)
			return SMT_FALSE;

		pLog->LogMessage("Init D3D 3DRenderDevice ok!");

		m_strLogName = logname;

		//////////////////////////////////////////////////////////////////////////
		if (NULL == ( m_pD3D = Direct3DCreate9( D3D_SDK_VERSION )))	
		{
			pLog->LogMessage( "Failed to create D3D object\n" );
			Release();

			return SMT_ERR_FAILURE;
		}

		//////////////////////////////////////////////////////////////////////////
		if(SMT_ERR_NONE !=SetDeviceCaps())
			return SMT_ERR_FAILURE;

		RECT wndRect;
		GetClientRect(m_hWnd,&wndRect);

		//////////////////////////////////////////////////////////////////////////
		// Build the creation parameters
		//--
		D3DDISPLAYMODE displayMode; 
		// 得到桌面显示模式
		if(FAILED(m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode)))  //GetAdapterDisplayMode获取当前显示器的显示模式
			return SMT_ERR_FAILURE; 

		// 设置D3DPRESENT_PARAMETERS结构以便创建D3D设备对象
		D3DPRESENT_PARAMETERS d3dpp; 
		ZeroMemory( &d3dpp, sizeof(D3DPRESENT_PARAMETERS) );
		d3dpp.Windowed			   = TRUE;
		d3dpp.hDeviceWindow        = hWnd;

		d3dpp.SwapEffect	       = D3DSWAPEFFECT_DISCARD;
		d3dpp.BackBufferCount	   = 1;
		//d3dpp.BackBufferWidth	   = wndRect.right;
		//d3dpp.BackBufferHeight   = wndRect.bottom;
		d3dpp.BackBufferFormat	   = displayMode.Format;
		d3dpp.EnableAutoDepthStencil = true;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

		// Try to require hardware T&L (Best case)
		//--
		if( FAILED( m_pD3D->CreateDevice( 
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &m_pD3DDevice ) ) )
		{
			pLog->LogMessage( "Failed to create D3D Device with D3DCREATE_HARDWARE_VERTEXPROCESSING\n" );

			// Failed ! try to get software T&L (Not that bad after all)
			//--
			if( FAILED( m_pD3D->CreateDevice( 
				D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &m_pD3DDevice ) ) )
			{
				pLog->LogMessage( "Failed to create D3D Device with D3DCREATE_SOFTWARE_VERTEXPROCESSING\n" );

				// Failed ! try to get software T&L AND reference device (Worst case)
				//--
				if( FAILED( m_pD3D->CreateDevice( 
					D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
					D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &m_pD3DDevice ) ) )
				{
					pLog->LogMessage( "Failed to create D3D Device with D3DCREATE_SOFTWARE_VERTEXPROCESSING and REF rasterizer. Buy another grphics card !!\n" );
					return SMT_ERR_FAILURE;
				}
			}
		}

		

		m_pStateManager = new SmtD3DGPUStateManager(m_pD3DDevice);

		//////////////////////////////////////////////////////////////////////////
		if( FAILED(D3DXCreateSprite( m_pD3DDevice, &m_pTextSprite )))
			return SMT_ERR_FAILURE;

		//////////////////////////////////////////////////////////////////////////
		RECT wndbfRect;
		Viewport3D viewport; 
		GetClientRect(m_hWnd,&wndbfRect);

		viewport.ulHeight = wndbfRect.bottom -wndbfRect.top;
		viewport.ulWidth = wndbfRect.right - wndbfRect.left;
		viewport.ulX = 0;
		viewport.ulY = 0;
		viewport.fZNear = 0;
		viewport.fZFar = 1;
		viewport.fFovy = 45.f;
		SetViewport(viewport);

		SetClearColor(SmtColor(1, 0, 0));
		SetDepthClearValue( 1.0f );
		SetStencilClearValue( 0 );

		m_pD3DDevice->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
		m_pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		m_pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		m_pD3DDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		m_pD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::Destroy()
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		//释放opengl资源
		//////////////////////////////////////////////////////////////////////////
		vector<string> vStrResNames;
		m_textureMgr.GetAllTextureName(vStrResNames);
		for (int i =0;i < vStrResNames.size() ;i++)
		{
			SmtTexture *pTexture = m_textureMgr.GetTexture(vStrResNames[i].c_str());
			if (pTexture)
			{
				uint handle = pTexture->GetHandle();
				if (handle != 0)
					;
					//glDeleteTextures(1, &handle);
			}
		}
		m_textureMgr.DestroyAllTexture();

		//////////////////////////////////////////////////////////////////////////
		m_shaderMgr.GetAllShaderName(vStrResNames);
		for (int i =0;i < vStrResNames.size() ;i++)
		{
			SmtShader *pShader = m_shaderMgr.GetShader(vStrResNames[i].c_str());
			if (pShader)
			{
				uint handle = pShader->GetHandle();
				if (handle != 0)
					m_pFuncShaders->glDeleteObject(handle);
			}
		}
		m_shaderMgr.DestroyAllShader();

		//////////////////////////////////////////////////////////////////////////
		m_progamMgr.GetAllProgramName(vStrResNames);
		for (int i =0;i < vStrResNames.size() ;i++)
		{
			SmtProgram *pProgram = m_progamMgr.GetProgram(vStrResNames[i].c_str());
			if (pProgram)
			{
				uint handle = pProgram->GetHandle();
				if (handle != 0)
					m_pFuncShaders->glDeleteObject(handle);
			}
		}
		m_progamMgr.DestroyAllProgram();

		//////////////////////////////////////////////////////////////////////////
		//
		SMT_SAFE_DELETE(m_pDeviceCaps);

		//
		SMT_SAFE_DELETE(m_pStateManager);
		//

		SMT_SAFE_RELEASE(m_pD3DDevice);
		SMT_SAFE_RELEASE(m_pD3D);
		SMT_SAFE_RELEASE(m_pTextSprite);

		return SMT_ERR_NONE;
	}


	long SmtD3DRenderDevice::Release()
	{
		for (int i = 0; i < m_vD3DFontPtrs.size();i++)
		{
			if(NULL != m_vD3DFontPtrs[i])
			{
				m_vD3DFontPtrs[i]->Release();
			}
		}

		m_vD3DFontPtrs.clear();

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetDeviceCaps(void)
	{
		m_pDeviceCaps = new SmtD3DDeviceCaps(this,m_pD3D);
		
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::ConvertType(Type type)
	{
		switch (type)
		{
		case SMT_SHORT:
			//return GL_SHORT;
		case SMT_INT:
			//return GL_INT;
		case SMT_FLOAT:
			//return GL_FLOAT;
		case SMT_DOUBLE:
			//return GL_DOUBLE;
		case SMT_UNSIGNED_INT:
			//return GL_UNSIGNED_INT;
		case SMT_UNSIGNED_BYTE:
			//return GL_UNSIGNED_BYTE;
		case SMT_UNSIGNED_SHORT:
			//return GL_UNSIGNED_SHORT;
			break;
		}
		return -1;
	}

	long SmtD3DRenderDevice::ConvertVideoBufferStoreMethod(VideoBufferStoreMethod method)
	{
		switch (method)
		{
		case STATIC_DRAW:
			//return GL_STATIC_DRAW_ARB;
		case STATIC_READ:
			//return GL_STATIC_READ_ARB;
		case STATIC_COPY:
			//return GL_STATIC_COPY_ARB;
		case DYNAMIC_DRAW:
			//return GL_DYNAMIC_DRAW_ARB;
		case DYNAMIC_READ:
			//return GL_DYNAMIC_READ_ARB;
		case DYNAMIC_COPY:
			//return GL_DYNAMIC_COPY_ARB;
		case STREAM_DRAW:
			//return GL_STREAM_DRAW_ARB;
		case STREAM_READ:
			//return GL_STREAM_READ_ARB;
		case STREAM_COPY:
			//return GL_STREAM_COPY_ARB;
			break;
		}

		return -1;
	}

	long SmtD3DRenderDevice::ConvertAccess(AccessMode access)
	{
		switch (access)
		{
		case READ_ONLY:
			//return GL_READ_ONLY_ARB;
		case WRITE_ONLY:
			//return GL_WRITE_ONLY_ARB;
		case READ_WRITE:
			//return GL_READ_WRITE_ARB;
			break;
		}
		return -1;
	}

	long SmtD3DRenderDevice::ConvertArrayType(ArrayType type)
	{
		switch (type)
		{
		case TEXTURE_COORD_ARRAY:
			//return GL_TEXTURE_COORD_ARRAY;
		case COLOR_ARRAY:
			//return GL_COLOR_ARRAY;
		case INDEX_ARRAY:
			//return GL_INDEX_ARRAY;
		case NORMAL_ARRAY:
			//return GL_NORMAL_ARRAY;
		case VERTEX_ARRAY:
			//return GL_VERTEX_ARRAY;
			break;
		}
		return -1;
	}

	long SmtD3DRenderDevice::ConvertTexFormat(TextureFormat format)
	{
		switch (format)
		{
		case RGB8:
			//return GL_RGB;
		case RGBA8:
			//return GL_RGBA;
		case RGB_DXT1:
			//return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		case RGBA_DXT1:
			//return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		case RGBA_DXT3:
			//return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		case RGBA_DXT5:
			//return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		case LUMINANCE8:
			//return GL_LUMINANCE;
		case INTENSITY8:
			//return GL_INTENSITY;
		case RGB16F:
			//return GL_RGB16F_ARB;
		case RGBA16F:
			//return GL_RGBA16F_ARB;
		case ALPHA16F:
			//return GL_ALPHA16F_ARB;
		case INTENSITY16F:
			//return GL_INTENSITY16F_ARB;
		case LUMINANCE16F:
			//return GL_LUMINANCE16F_ARB;
		case LUMINANCE_ALPHA16F:
			//return GL_LUMINANCE_ALPHA16F_ARB;
		case RGB32F:
			//return GL_RGB32F_ARB;
		case RGBA32F:
			//return GL_RGBA32F_ARB;
		case ALPHA32F:
			//return GL_ALPHA32F_ARB;
		case INTENSITY32F:
			//return GL_INTENSITY32F_ARB;
		case LUMINANCE32F:
			//return GL_LUMINANCE32F_ARB;
		case LUMINANCE_ALPHA32F:
			//return GL_LUMINANCE_ALPHA32F_ARB;
			break;
		}

		return -1;
	}

}