#include "sde_mem.h"
#include "smt_api.h"
#include "gis_api.h"
#include "bl_api.h"
#include "sde_mem.h"
#include "ximage.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

namespace Smt_SDEMem
{
	SmtMemRasLayer::SmtMemRasLayer(SmtDataSource *pOwnerDs):SmtRasterLayer(pOwnerDs)
	{
		m_pOwnerDs = pOwnerDs->Clone();

		sprintf(m_szLayerName,"DefRaster");  
	
		m_bIsVisible  = true;

		m_pRasterBuf = NULL;
		m_lRasterBufSize  = 0;
		m_lCodeType = -1;

		m_fRasterRect.lb.x = 0;
		m_fRasterRect.lb.y = 0;

		m_fRasterRect.rt.x = 800;
		m_fRasterRect.rt.y = 600;
	}

	SmtMemRasLayer::~SmtMemRasLayer(void)
	{
		SMT_SAFE_DELETE(m_pOwnerDs);
		if (IsOpen())
			Close();
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtMemRasLayer::Open(const char *szName) 
	{ 
		m_bOpen = true;
		strcpy(m_szLayerName,szName);

		return m_bOpen;
	}

	bool SmtMemRasLayer::Create(void)
	{
		return true;
	}

	bool SmtMemRasLayer::Close(void)
	{
		m_bOpen = false;
		return true;
	}

	bool SmtMemRasLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	void SmtMemRasLayer::CalEnvelope(void)
	{
		RectToEnvelope(m_lyrEnv,m_fRasterRect);
	}

	//////////////////////////////////////////////////////////////////////////
	//raster oper
	long SmtMemRasLayer::SetRasterRect(const fRect &fLocRect)
	{
		m_fRasterRect  = fLocRect;

		return SMT_ERR_NONE;
	}

	long SmtMemRasLayer::CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fRasterRect,long lImageCode)
	{
		SMT_SAFE_DELETE_A(m_pRasterBuf);
		m_lRasterBufSize = 0;

		m_pRasterBuf = new char[lRasterBufSize];
		memcpy(m_pRasterBuf,pRasterBuf,lRasterBufSize);

		m_lRasterBufSize = lRasterBufSize;
		m_lCodeType	   = lImageCode;
		m_fRasterRect  = fRasterRect;

		CalEnvelope();

		return SMT_ERR_NONE;
	}

	long SmtMemRasLayer::GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		pRasterBuf = new char[m_lRasterBufSize];
		memcpy(pRasterBuf,m_pRasterBuf,m_lRasterBufSize);

		lRasterBufSize = m_lRasterBufSize;
		lImageCode	   = m_lCodeType;
		fRasterRect	   = m_fRasterRect;

		return SMT_ERR_NONE;
	}

	long SmtMemRasLayer::GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		pRasterBuf	   = m_pRasterBuf;
		lRasterBufSize = m_lRasterBufSize;
		lImageCode	   = m_lCodeType;
		fRasterRect	   = m_fRasterRect;

		return SMT_ERR_NONE;
	}

	long SmtMemRasLayer::GetRasterRect(fRect &fLocRect) const
	{
		fLocRect = m_fRasterRect;

		return SMT_ERR_NONE;
	}
}