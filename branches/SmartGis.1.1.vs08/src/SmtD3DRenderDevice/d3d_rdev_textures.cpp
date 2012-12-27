#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"
using namespace Smt_Core;

namespace Smt_3Drd
{
	SmtTexture* SmtD3DRenderDevice::CreateTexture(const char *szName)
	{
		uint unHandle;
		//glGenTextures( 1, &unHandle);

		SmtTexture *pTex = new SmtTexture(this,unHandle,szName);
		if (SMT_ERR_NONE == m_textureMgr.AddTexture(pTex))
			return pTex;
		else
		{
			SMT_SAFE_DELETE(pTex);
			return NULL;
		}
	}

	long SmtD3DRenderDevice::GenerateMipmap(SmtTexture *pTexture)
	{
		if (NULL == pTexture)
			return SMT_ERR_INVALID_PARAM;

		if (SMT_ERR_NONE == pTexture->Use())
		{
			//m_pFuncMipmap->glGenerateMipmap(GL_TEXTURE_2D);
			pTexture->Unuse();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtD3DRenderDevice::DestroyTexture(const char *szName)
	{
		SmtTexture *pTexture = m_textureMgr.GetTexture(szName);

		if (NULL == pTexture)
			return SMT_ERR_INVALID_PARAM;
		 
		uint handle = pTexture->GetHandle();
		if (handle != 0)
		{
			//glDeleteTextures(1, &handle);
		}

		m_textureMgr.DestroyTexture(pTexture->GetTextureName());

		return SMT_ERR_NONE;
	}

	SmtTexture* SmtD3DRenderDevice::GetTexture(const char *szName)
	{
		return m_textureMgr.GetTexture(szName);
	}

	long SmtD3DRenderDevice::BindTexture(SmtTexture *pTexture)
	{
		if (NULL == pTexture)
			return SMT_ERR_INVALID_PARAM;

		uint handle = pTexture->GetHandle();
		//glBindTexture(GL_TEXTURE_2D, handle);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::BuildTexture(SmtTexture *pTexture)
	{
		if (NULL == pTexture)
			return SMT_ERR_INVALID_PARAM;

		void  *pDataBuf = pTexture->GetData();

		if (NULL == pDataBuf)
			return SMT_ERR_INVALID_PARAM;

		TextureDesc texDesc = pTexture->GetDesc();

		uint  internalFormat = ConvertTexFormat(texDesc.format);
		uint  nativeFormat = 0;
		int	  components = pTexture->GetComponents(texDesc.format);

		//uint  target = GL_TEXTURE_2D;

		//bool bPowerOfTwo =  pTexture->IsSizePowerOfTwo();

		///*if (!bPowerOfTwo)
		//target = GL_TEXTURE_RECTANGLE_ARB;*/

		//if (components == 1)
		//	nativeFormat = GL_LUMINANCE;
		//else
		//{
		//	if (components == 3)
		//		nativeFormat = GL_RGB;
		//	else
		//		nativeFormat = GL_RGBA;
		//}

		//TextureSampler texSampler;
		//TextureEnvMode texEvn;
		//texSampler.rTexture = REPEAT;
		//texSampler.sTexture = REPEAT;
		//texSampler.tTexture = REPEAT;
		//texEvn.envMode		= MODULATE;

		////
		//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		//glBindTexture(target, pTexture->GetHandle());

		//if (pTexture->IsUseMipmap())
		//{
		//	texSampler.magFilter = LINEAR_MIPMAP_LINEAR;
		//	texSampler.minFilter = LINEAR_MIPMAP_LINEAR;

		//	gluBuild2DMipmaps(target,components,texDesc.width,texDesc.height,nativeFormat, GL_UNSIGNED_BYTE, pDataBuf );

		//	//m_p3DRenderDevice->GenerateMipmap(this);
		//}
		//else
		//{
		//	texSampler.magFilter = LINEAR;
		//	texSampler.minFilter = LINEAR;

		//	glTexImage2D(target, 0, internalFormat,texDesc.width,texDesc.height, 0, nativeFormat, GL_UNSIGNED_BYTE,pDataBuf);
		//}

		//pTexture->SetEnvMode(texEvn);
		//pTexture->SetSampler(texSampler);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UnbindTexture()
	{
		//glBindTexture(GL_TEXTURE_2D, 0);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::BindRectTexture(SmtTexture *pTexture)
	{
		if (NULL == pTexture)
			return SMT_ERR_INVALID_PARAM;

		//uint handle = pTexture->GetHandle();
		//glBindTexture(GL_TEXTURE_RECTANGLE_ARB, handle);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UnbindRectTexture()
	{
		//glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

		return SMT_ERR_NONE;
	}
}