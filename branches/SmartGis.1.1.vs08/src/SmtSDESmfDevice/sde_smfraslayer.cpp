#include "sde_smf.h"
#include "smt_api.h"
#include "gis_api.h"
#include "bl_api.h"
#include "sde_mem.h"
#include "ximage.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

namespace Smt_SDESmf
{
	SmtSmfRasLayer::SmtSmfRasLayer(SmtDataSource *pOwnerDs):SmtRasterLayer(pOwnerDs)
	{
		m_pOwnerDs = pOwnerDs->Clone();
		sprintf(m_szLayerName,"DefRaster");  
		m_pAtt = new SmtAttribute();

		SmtDataSource *pDS = new SmtMemDataSource();
		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			m_pMemLayer = pDS->CreateRasterLayer("SmtMemRasLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);
	}

	SmtSmfRasLayer::~SmtSmfRasLayer(void)
	{
		SMT_SAFE_DELETE(m_pOwnerDs);

		//////////////////////////////////////////////////////////////////////////
		//mem
		SMT_SAFE_DELETE(m_pMemLayer);
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtSmfRasLayer::Create(void)
	{
		sprintf(m_szLayerFileName,m_szLayerName);

		return m_pMemLayer->Create();
	}

	bool SmtSmfRasLayer::Open(const char * szLayerFileName)
	{
		sprintf(m_szLayerFileName,szLayerFileName);

		m_bOpen = m_pMemLayer->Open(m_szLayerFileName);

		return m_bOpen;
	}

	bool SmtSmfRasLayer::Close(void)
	{
		m_bOpen = m_pMemLayer->Close();
		return m_bOpen;
	}

	bool SmtSmfRasLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		char szImgUrl[MAX_LAYER_FILE_NAME];

		SmtDataSourceInfo info;
		m_pOwnerDs->GetInfo(info);

		sprintf(szImgUrl,"%s",m_szLayerFileName);

		long lCodeType = GetImageTypeByFileExt(m_szLayerFileName);

		CxImage img;

		if (img.Load(szImgUrl,lCodeType))
		{
			BYTE *pImageBuf = NULL;
			long lImageBufSize = 0;
			fRect fLocRect;

			m_pMemLayer->GetRasterRect(fLocRect);
	
			fLocRect.rt.y  = fLocRect.lb.y + fLocRect.Width()*img.GetHeight()/img.GetWidth();

			if (img.Encode(pImageBuf,lImageBufSize,lCodeType))
			{
				return (SMT_ERR_NONE == m_pMemLayer->CreaterRaster((const char *)pImageBuf,lImageBufSize,fLocRect,lCodeType));
			}
		}

		return false;
	}

	void SmtSmfRasLayer::CalEnvelope(void)
	{
		Envelope env;

		m_pMemLayer->CalEnvelope();
		m_pMemLayer->GetEnvelope(env);

		memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	}

	void  SmtSmfRasLayer::SetLayerRect(const fRect &lyrRect)
	{
		m_pMemLayer->SetLayerRect(lyrRect);
		memcpy(&m_lyrEnv,&lyrRect,sizeof(fRect));
	}

	//////////////////////////////////////////////////////////////////////////
	//raster oper
	long SmtSmfRasLayer::SetRasterRect(const fRect &fLocRect)
	{
		return m_pMemLayer->SetRasterRect(fLocRect);
	}

	long SmtSmfRasLayer::CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fRasterRect,long lImageCode)
	{
		long lRtn = m_pMemLayer->CreaterRaster(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);

		CalEnvelope();

		return lRtn;
	}

	long SmtSmfRasLayer::GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		return m_pMemLayer->GetRaster(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);
	}

	long SmtSmfRasLayer::GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		return m_pMemLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);
	}

	long SmtSmfRasLayer::GetRasterRect(fRect &fLocRect) const
	{
		return m_pMemLayer->GetRasterRect(fLocRect);
	}
}