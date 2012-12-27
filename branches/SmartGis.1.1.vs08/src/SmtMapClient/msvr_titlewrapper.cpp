#include "msvr_mapclient.h"
#include "sys_sysmanager.h"
#include "smt_logmanager.h"
#include "bl_api.h"
#include "smt_api.h"
#include "ximage.h"

using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_GIS;
using namespace Smt_Rd;
using namespace Smt_Sys;
using namespace Smt_Base;
using namespace Smt_MapService;

namespace Smt_MapClient
{
	//////////////////////////////////////////////////////////////////////////
	//SmtTileWrapper
	//////////////////////////////////////////////////////////////////////////
	SmtTileWrapper::SmtTileWrapper():m_pBindService(NULL)
		,m_pBindTileLayer(NULL)
		,m_lTileImageCode(-1)
		,m_lZoom(-1)
	{

	}

	SmtTileWrapper::~SmtTileWrapper()
	{
		Destroy();
	}

	long SmtTileWrapper::Init(SmtMapService *pBindService,SmtTileLayer *pTarTileLayer)
	{
		if (SMT_ERR_NONE != Destroy())
			return SMT_ERR_FAILURE;

		m_pBindService = pBindService;
		m_pBindTileLayer = pTarTileLayer;

		return SMT_ERR_NONE;
	}

	long SmtTileWrapper::Create(void)
	{
		if (NULL == m_pBindService || NULL == m_pBindTileLayer)
			return SMT_ERR_FAILURE;

		//获取图像编码格式及瓦片尺寸
		m_pBindService->GetTileSize(m_lTileWidth,m_lTileHeight);
		m_lTileImageCode = m_pBindService->GetImageCode();

		//初始zoom
		Envelope	llEnv;

		m_pBindService->GetEnvelope(llEnv);
		m_lZoom = m_pBindService->GetZoomMin();
		m_center.x =  (llEnv.MinX + llEnv.MaxX)/2.;
		m_center.y =  (llEnv.MinY + llEnv.MaxY)/2.;
		
		return SMT_ERR_NONE;
	}

	long SmtTileWrapper::Destroy()
	{
		m_pBindService = NULL;
		m_pBindTileLayer = NULL;

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	inline long SmtTileWrapper::SetClientRect(const lRect &lClientRect)
	{
		m_lClientRect = lClientRect;

		return SMT_ERR_NONE;
	}

	void SmtTileWrapper::GetClientLBRect(Smt_Core::fRect &rct)
	{
		double		dbfXPixelRes = 1.,dbfYPixelRes = 1.;
		
		dbfXPixelRes = m_pBindService->GetXPixelResolution(m_lZoom);
		dbfYPixelRes = m_pBindService->GetYPixelResolution(m_lZoom);

		rct.lb.x = m_center.x - m_lClientRect.Width()*dbfXPixelRes/2;
		rct.lb.y = m_center.y - m_lClientRect.Height()*dbfYPixelRes/2;
		rct.rt.x = m_center.x + m_lClientRect.Width()*dbfXPixelRes/2;
		rct.rt.y = m_center.y + m_lClientRect.Height()*dbfYPixelRes/2;
	}

	long SmtTileWrapper::GetTileRange(long &lMinCol,long &lMinRow,long &lMaxCol,long &lMaxRow) const
	{
		if (NULL == m_pBindService)
			return SMT_ERR_FAILURE;

		Envelope	llEnv;

		//计算title范围
		m_pBindService->GetEnvelope(llEnv);
		m_pBindService->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow,llEnv,m_lZoom);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	inline long SmtTileWrapper::SetZoomCenter(const Smt_Core::fPoint &center)
	{
		m_center = center;

		return SMT_ERR_NONE;
	}

	inline long SmtTileWrapper::GetZoomCenter(Smt_Core::fPoint &center) const
	{
		center = m_center;

		return SMT_ERR_NONE;
	}

	inline long SmtTileWrapper::SetZoom(long lZoom,const Smt_Core::fPoint &orgPoint)
	{
		if (NULL == m_pBindService || NULL == m_pBindTileLayer)
			return SMT_ERR_FAILURE;

		if (lZoom < m_pBindService->GetZoomMin() ||
			lZoom > m_pBindService->GetZoomMax())
			return SMT_ERR_FAILURE;

		if (m_lZoom == lZoom )
			return SMT_ERR_NONE;

		m_lZoom = lZoom;

		return SetZoomCenter(orgPoint);
	}

	inline long SmtTileWrapper::SetZoom(long lZoom)
	{
		return SetZoom(lZoom,m_center);
	}

	inline long SmtTileWrapper::GetZoom(void) const
	{
		return m_lZoom;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtTileWrapper::Sync2TileLayer()
	{
		if (NULL == m_pBindService || NULL == m_pBindTileLayer)
			return SMT_ERR_FAILURE;

		if (!m_pBindTileLayer->IsOpen())
			m_pBindTileLayer->Open("");
		 
		//获取视口逻辑范围
		Envelope vpEnv;
		fRect	 vpRect;
		GetClientLBRect(vpRect);
		RectToEnvelope(vpEnv,vpRect);

		//计算title范围
		long		lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope	llEnv;
		fRect		llRect;

		m_pBindService->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow,vpEnv,m_lZoom);

		char	 *pTileImgBuf = NULL;
		long	 lTileImgBufSize = 0;

		Envelope titleEnv;
		fRect    titleRect;
		long	 lUID = 0;

		for (int i = lMinCol; i <= lMaxCol;i++)
		{
			for(int j = lMinRow;j <= lMaxRow;j++)
			{
				m_pBindService->GetTileLongLatRange(titleEnv,i,j,m_lZoom);

				//是否在当前视口范围内
				if (!titleEnv.Intersects(vpEnv))
					continue;

				//是否已经加载数据
				lUID = TITLE_GETUNIID(m_lZoom,i,j);

				if (NULL != m_pBindTileLayer->GetTileByID(lUID))
					continue;

				if (SMT_ERR_NONE != m_pBindService->GetTileImage(pTileImgBuf,lTileImgBufSize,i,j,m_lZoom))
					continue;

				EnvelopeToRect(titleRect,titleEnv);

				SmtTile title;
				title.lID = lUID;
				title.lImageCode = m_pBindService->GetImageCode();
				title.rtTileRect = titleRect;
				title.pTileBuf = pTileImgBuf;
				title.lTileBufSize = lTileImgBufSize;
				m_pBindTileLayer->AppendTile(&title,true);

				m_pBindService->ReleaseTileImage(pTileImgBuf);
			}
		}

		//控制显示
		m_pBindTileLayer->MoveFirst();
		while (!m_pBindTileLayer->IsEnd())
		{
			SmtTile *pTile = m_pBindTileLayer->GetTile();
			if (NULL != pTile)
			{
				RectToEnvelope(titleEnv,pTile->rtTileRect);
				pTile->bVisible = ((TITLE_GETZOOM(pTile->lID) == m_lZoom) && titleEnv.Intersects(vpEnv));
			}			

			m_pBindTileLayer->MoveNext();
		}

		return SMT_ERR_NONE;
	}
}