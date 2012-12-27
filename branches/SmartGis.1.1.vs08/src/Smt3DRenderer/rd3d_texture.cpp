#include "rd3d_texture.h"
#include "rd3d_3drenderdevice.h"
#include "smt_api.h"
#include "ximage.h"
#include "smt_logmanager.h"

namespace Smt_3Drd
{
	SmtTexture::SmtTexture (LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName):m_unHandle(handle)
		,m_p3DRenderDevice(p3DRenderDevice)
		,m_strName(strName)
		,m_bDynamic(false)
		,m_bUseMips(false)
		,m_ulPixelStride(0)
		,m_bLocked(false)
		,m_pBuffer(NULL)
		,m_pCurrentPixel(NULL)
	{
		;
	}

	SmtTexture::~SmtTexture()
	{
		;
	}

	long SmtTexture::Use(void)
	{
		SmtGPUStateManager *stateManager = m_p3DRenderDevice->GetStateManager();

		if (SMT_ERR_NONE == m_p3DRenderDevice->BindTexture(this) &&
			SMT_ERR_NONE == stateManager->SetSampler(m_texSampler) &&
			SMT_ERR_NONE == stateManager->SetTextureEnvironment(m_texEnv))
		{
			return SMT_ERR_NONE;
		}
		
		return SMT_ERR_FAILURE;
	}

	long SmtTexture::Unuse()
	{
		if (SMT_ERR_NONE == m_p3DRenderDevice->UnbindTexture())
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	TextureSampler SmtTexture::GetSampler() const
	{
		return m_texSampler;
	}

	void SmtTexture::SetSampler(TextureSampler sampler)
	{
		m_texSampler = sampler;
	}

	TextureEnvMode SmtTexture::GetEnvMode() const
	{
		return m_texEnv;
	}

	void SmtTexture::SetEnvMode(TextureEnvMode envMode) 
	{
		m_texEnv = envMode;
	}

	TextureDesc SmtTexture::GetDesc() const
	{
		return m_texDesc;
	}

	int SmtTexture::GetComponents(TextureFormat format)
	{
		switch (format)
		{
		case ALPHA16F:
		case ALPHA32F:
		case INTENSITY8:
		case INTENSITY16F:
		case INTENSITY32F:
		case LUMINANCE8:
		case LUMINANCE16F:
		case LUMINANCE32F:
			return 1;

		case LUMINANCE_ALPHA16F:
		case LUMINANCE_ALPHA32F:
			return 2;

		case RGB8:
		case RGB16F:
		case RGB32F:
		case RGB_DXT1:
			return 3;

		case RGBA8:
		case RGBA_DXT1:
		case RGBA_DXT3:
		case RGBA_DXT5:
		case RGBA16F:
		case RGBA32F:
			return 4;

		default:
			return 0;
		}
	}

	bool SmtTexture::IsSizePowerOfTwo()
	{
		return (((m_texDesc.width  & (m_texDesc.width  - 1)) == 0) && 
				((m_texDesc.height & (m_texDesc.height - 1)) == 0));
	}

	long SmtTexture::Create(ulong ulWidth, ulong ulHeight,TextureFormat format,bool bDynamic,bool bUseMips)
	{
		m_texDesc.width  = ulWidth;
		m_texDesc.height = ulHeight;
		m_texDesc.format = format;
		m_bUseMips = bUseMips;

		if( format == RGB8 )
			m_ulPixelStride = 3;
		else
			m_ulPixelStride = 4;

		m_bDynamic    = bDynamic;
		m_bLocked     = false;

		m_pBuffer = (void*)(new unsigned char[m_ulPixelStride*m_texDesc.width*m_texDesc.height]);
		ZeroMemory( m_pBuffer, m_ulPixelStride*m_texDesc.width*m_texDesc.height);

		m_pCurrentPixel = (unsigned char*)NULL;

		return SMT_ERR_NONE;
	}

	long SmtTexture::SetData(void *pData,ulong ulSize)
	{
		if (!IsLocked())
			return SMT_ERR_FAILURE;
		 
		if(m_pBuffer == NULL ||pData == NULL || ulSize < 1 || 
			ulSize != m_ulPixelStride*m_texDesc.width*m_texDesc.height) 
			return SMT_ERR_FAILURE;

		memcpy(m_pBuffer, pData,ulSize);

		return SMT_ERR_NONE;
	}

	void *SmtTexture::GetData()
	{
		return m_pBuffer;
	}

	long SmtTexture::Lock()
	{
		m_pCurrentPixel= (unsigned char*)m_pBuffer;
		m_bLocked = true;

		return SMT_ERR_NONE;
	}

	long SmtTexture::Unlock()
	{
		m_bLocked = false;
		m_pCurrentPixel = (unsigned char*)NULL;

		if ( m_pBuffer!=NULL )
		{
			m_p3DRenderDevice->BuildTexture(this);
			//...

			SMT_SAFE_DELETE_A(m_pBuffer);
			m_pCurrentPixel= (unsigned char*)m_pBuffer;
		}

		return SMT_ERR_NONE;
	}

	long SmtTexture::Load(string fileName,bool bDynamic,bool bUseMips)
	{
		int				nImageTyle = GetImageTypeByFileExt(fileName.c_str());
		TextureFormat	texFmt;
		CxImage  img(fileName.c_str(), nImageTyle);

		if (img.IsValid())
		{
			if(img.GetBpp() == 24)
				texFmt= RGB8;
			else
				texFmt= RGBA8;

			if (SMT_ERR_NONE == Create(img.GetWidth(),img.GetHeight(), texFmt,bDynamic,bUseMips) &&
				SMT_ERR_NONE == Lock() &&
				SMT_ERR_NONE == SetData(img.GetDIB(),img.GetHeight()*img.GetWidth()*(img.GetBpp()/8)) &&
				SMT_ERR_NONE == Unlock())
			{
				return SMT_ERR_NONE;
			}
			else
			{
				SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
				SmtLog *pLog = pLogMgr->GetDefaultLog();

				pLog->LogMessage("AddTexture() %s fail",fileName.c_str());

				return SMT_ERR_FAILURE;
			}
		}

		return SMT_ERR_FAILURE;
	}
}