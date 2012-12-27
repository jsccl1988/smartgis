/*
File:    rd3d_texture.h

Desc:    SmtTexture, texture ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_TEXTURE_H
#define _RD3D_TEXTURE_H

#include "rd3d_3drenderdefs.h"

namespace Smt_3Drd
{
	/**
	Supported texture formats.
	*/
	enum TextureFormat
	{
		RGB8,           
		RGBA8,          
		RGB_DXT1,       
		RGBA_DXT1,      
		RGBA_DXT3,      
		RGBA_DXT5,      
		LUMINANCE8,     
		INTENSITY8,     
		RGB16F,         
		RGBA16F,        
		ALPHA16F,       
		INTENSITY16F,   
		LUMINANCE16F,   
		LUMINANCE_ALPHA16F, 
		RGB32F,         
		RGBA32F,        
		ALPHA32F,       
		INTENSITY32F,   
		LUMINANCE32F,   
		LUMINANCE_ALPHA32F 
	};

	enum TextureEnvironment
	{
		REPLACE = 0,
		MODULATE,
		DECAL,
		BLEND,
		ADD
	};

	enum TextureFilter
	{
		NEAREST = 0,
		LINEAR,
		NEAREST_MIPMAP_NEAREST,     // For minFilter only
		LINEAR_MIPMAP_NEAREST,      // For minFilter only
		NEAREST_MIPMAP_LINEAR,      // For minFilter only
		LINEAR_MIPMAP_LINEAR        // For minFilter only
	};

	/**
	Texture loading mode. Defines compression type
	during texture loading.
	*/
	enum TextureLoadMode
	{
		TLM_UNCOMPRESSED = 0,
		TLM_DXT1,
		TLM_DXT3,
		TLM_DXT5
	};

	/**
	Defines behavior of texture mapping when texture coords are out of range.
	It is set per texture coordinate and may be different for different coordinates.
	*/
	enum TextureWrapMode
	{
		CLAMP = 0,
		REPEAT,
		CLAMP_TO_EDGE
	};

	/**
	Memory pool for texture storing.
	*/
	enum Pool
	{
		P_MANAGED,
		P_DEFAULT,
		P_SYSMEMORY
	};

	enum Usage
	{
		U_UNDEFINED
	};

	struct TextureDesc
	{
		TextureFormat	format;
		int				width;
		int				height;
		Pool			pool;
		Usage			usage;
	};

	struct TextureSampler
	{
		TextureFilter	minFilter;
		TextureFilter	magFilter;
		float			anisotropy;
		TextureWrapMode sTexture;
		TextureWrapMode tTexture;
		TextureWrapMode rTexture;

		/**
		Default constructor. Sets both filters to bilinear.
		*/
		TextureSampler()
		{
			/* Binilear filtering */
			minFilter = LINEAR;
			magFilter = LINEAR;

			/* Anisotropy is off */
			anisotropy = 0.0;

			/* Texture wrap modes */
			sTexture = CLAMP;
			tTexture = CLAMP;
			rTexture = CLAMP;
		}

		TextureSampler(TextureFilter minFilter, TextureFilter magFilter)
		{
			/* Set filtering */
			this->minFilter = minFilter;
			this->magFilter = magFilter;

			/* Anisotropy is off */
			anisotropy = 0.0;

			/* Texture wrap modes */
			sTexture = CLAMP;
			tTexture = CLAMP;
			rTexture = CLAMP;
		}
	};

	struct TextureEnvMode
	{
		TextureEnvironment envMode;

		TextureEnvMode()
		{
			envMode = REPLACE;
		}

		TextureEnvMode(TextureEnvironment theEnvMode)
		{
			envMode = theEnvMode;
		}
	};
	
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtTexture
	{
	public:
		SmtTexture(LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName);
		virtual ~SmtTexture();

	public:
		inline uint				GetHandle() {return m_unHandle;}
		const char				*GetTextureName(void) {return m_strName.c_str();}

	public:
		TextureSampler			GetSampler() const;
		void					SetSampler(TextureSampler sampler);

		TextureEnvMode			GetEnvMode() const;
		void					SetEnvMode(TextureEnvMode envMode);

		TextureDesc				GetDesc() const;

	
	public:
		//////////////////////////////////////////////////////////////////////////
		//1.
		long					Load(string fileName,bool bDynamic = false,bool bUseMips = true);

		//////////////////////////////////////////////////////////////////////////
		//2.
		long					Create(ulong ulWidth, ulong ulHeight,TextureFormat format,bool bDynamic = false,bool bUseMips = true);

		long					Lock(void);
		long					Unlock(void);
		inline bool				IsLocked(void) const { return m_bLocked; }

		inline	bool			IsUseMipmap(void)const { return m_bUseMips; }

	public:
		long					Use();
		long					Unuse();

	public:
		int						GetComponents(TextureFormat format);
		bool					IsSizePowerOfTwo(void);

		void*					GetData();
		long					SetData(void* pData,ulong ulSize);

		void					SetPixel4f( float a, float r, float g, float b );
		void					SetPixel3f( float r, float g, float b );
		void					SetPixel4uc( unsigned char a, unsigned char r, unsigned char g, unsigned char b );
		void					SetPixel3uc( unsigned char r, unsigned char g, unsigned char b );

	protected:
		TextureSampler			m_texSampler;
		TextureEnvMode			m_texEnv;
		TextureDesc				m_texDesc;

		bool					m_bDynamic;				
		bool					m_bUseMips;	

		ulong					m_ulPixelStride;		
		bool					m_bLocked;			

		void*					m_pBuffer;		
		unsigned char*			m_pCurrentPixel;

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		uint					m_unHandle;
		string					m_strName;	
	};

	inline void SmtTexture::SetPixel4f( float a, float r, float g, float b )
	{
		if ( m_texDesc.format==RGBA8 )
		{
			m_pCurrentPixel[0] = (unsigned char)(255.0f*r);
			m_pCurrentPixel[1] = (unsigned char)(255.0f*g);
			m_pCurrentPixel[2] = (unsigned char)(255.0f*b);
			m_pCurrentPixel[3] = (unsigned char)(255.0f*a);
			m_pCurrentPixel+=4;
		}
	}

	inline void SmtTexture::SetPixel3f(float r, float g, float b )
	{
		if ( m_texDesc.format== RGB8 )
		{
			m_pCurrentPixel[0] = (unsigned char)(255.0f*r);
			m_pCurrentPixel[1] = (unsigned char)(255.0f*g);
			m_pCurrentPixel[2] = (unsigned char)(255.0f*b);
			m_pCurrentPixel+=3;
		}
	}

	inline void SmtTexture::SetPixel4uc( 
		unsigned char a, 
		unsigned char r, 
		unsigned char g, 
		unsigned char b )
	{
		if ( m_texDesc.format==RGBA8 )
		{
			m_pCurrentPixel[0] = r;
			m_pCurrentPixel[1] = g;
			m_pCurrentPixel[2] = b;
			m_pCurrentPixel[3] = a;
			m_pCurrentPixel+=4;
		}
	}

	inline void SmtTexture::SetPixel3uc( 
		unsigned char r, 
		unsigned char g, 
		unsigned char b )
	{
		if ( m_texDesc.format==RGB8 )
		{
			m_pCurrentPixel[0] = r;
			m_pCurrentPixel[1] = g;
			m_pCurrentPixel[2] = b;
			m_pCurrentPixel+=3;
		}
	}
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_TEXTURE_H