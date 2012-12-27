#include "msvr_mapclient.h"
#include "gis_api.h"
#include "bl_api.h"

using namespace Smt_Base;
using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_MapService;

namespace Smt_MapClient
{
	SmtWSMap::SmtWSMap(void):m_lZoom(0)
	{
		;
	}

	SmtWSMap::~SmtWSMap(void)
	{
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}

		m_vTileWrappers.clear();

		m_pCurrentLayer = NULL;
	}

	bool SmtWSMap::AddTileLayer(const SmtLayer * pLayer,const SmtMapService *pBindService)
	{
		if (pLayer->GetLayerType() != LYR_TITLE)
			return false;
	
		if (SmtMap::AddLayer(pLayer))
		{
			SmtTileWrapper *pTileWrapper = new SmtTileWrapper;

			if (SMT_ERR_NONE == pTileWrapper->Init(const_cast<SmtMapService *>(pBindService),(SmtTileLayer*)(const_cast<SmtLayer *>(pLayer))))
			{
				m_vTileWrappers.push_back(pTileWrapper);

				return true;
			}
			else
			{
				SmtMap::DeleteLayer(pLayer->GetLayerName());

				return false;
			}
		}

		return false;
	}

	bool SmtWSMap::RemoveTileLayer(const char * szName)
	{
		SmtTileWrapper *pTileWrapper = GetTileWrapper(szName);
		if (NULL == pTileWrapper)
			return false;
		
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			if ((*iter) == pTileWrapper)
			{
				SmtMap::DeleteLayer(pTileWrapper->GetBindTileLayer()->GetLayerName());

				SMT_SAFE_DELETE(*iter);

				m_vTileWrappers.erase(iter);

				return true;
			}

			++iter;
		} 

		return false;
	}

	bool SmtWSMap::QueryFeature(const SmtGQueryDesc *  pC_GQueryDesc,const SmtPQueryDesc * pC_PQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType)
	{
		bool bRet = false;

		SmtGQueryDesc *pGQueryDesc = const_cast<SmtGQueryDesc *>(pC_GQueryDesc);
		SmtPQueryDesc *pPQueryDesc = const_cast<SmtPQueryDesc *>(pC_PQueryDesc);

		if (m_pCurrentLayer)
		{
			if (LYR_TITLE == m_pCurrentLayer->GetLayerType())
			{
				SmtTileWrapper *pTileWrapper = GetTileWrapper(m_pCurrentLayer->GetLayerName());
				if (NULL == pTileWrapper)
					return false;

				SmtMapService	*pBindService = pTileWrapper->GetBindService();

				if (NULL == pBindService)
					return false;

				return pBindService->QueryFeature(pGQueryDesc,pPQueryDesc,pQueryResult,nFeaType);
			}
			else
			{
				SmtVectorLayer *pVLayer = (SmtVectorLayer *)m_pCurrentLayer;
				nFeaType = pVLayer->GetLayerFeatureType();
				pGQueryDesc->sSRs = (SmtSpatialRs)GetQueryRs(pGQueryDesc->pQueryGeom->GetGeometryType(),nFeaType);
				bRet = (pVLayer->Query(pGQueryDesc,pPQueryDesc,pQueryResult) == SMT_ERR_NONE);
			}
		}		

		return bRet;
	}

	SmtTileWrapper* SmtWSMap::GetTileWrapper(const char * szName)
	{
		SmtLayer *pLayer = SmtMap::GetLayer(szName);

		if (NULL == pLayer ||pLayer->GetLayerType() != LYR_TITLE)
			return NULL;

		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			if ((*iter)->GetBindTileLayer() == pLayer)
			{
				return (*iter);
			}
				
			++iter;
		}

		return NULL;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	inline long SmtWSMap::SetClientRect(const lRect &lClientRect)
	{
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			(*iter)->SetClientRect(lClientRect);
			++iter;
		}

		m_lClientRect = lClientRect;

		return SMT_ERR_NONE;
	}

	void SmtWSMap::GetClientLBRect(Smt_Core::fRect &rct)
	{
		Envelope	mapEnv;

		fRect		lyrRect;
		Envelope	lyrEnv;

		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			(*iter)->GetClientLBRect(lyrRect);
			RectToEnvelope(lyrEnv,lyrRect);
			mapEnv.Merge(lyrEnv);

			++iter;
		}

		EnvelopeToRect(rct,mapEnv);
	}

	//////////////////////////////////////////////////////////////////////////
	inline long SmtWSMap::SetZoomCenter(const Smt_Core::fPoint &center)
	{
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			(*iter)->SetZoomCenter(center);
			++iter;
		}

		m_center = center;

		return SMT_ERR_NONE;
	}

	inline long SmtWSMap::GetZoomCenter(Smt_Core::fPoint &center) const
	{
		center = m_center;

		return SMT_ERR_NONE;
	}

	inline long SmtWSMap::SetZoom(long lZoom,const Smt_Core::fPoint &orgPoint)
	{
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			(*iter)->SetZoom(lZoom,orgPoint);
			++iter;
		}
		
		m_lZoom = lZoom;

		return SetZoomCenter(orgPoint);
	}

	inline long SmtWSMap::SetZoom(long lZoom)
	{
		return SetZoom(lZoom,m_center);
	}

	inline long SmtWSMap::GetZoom(void) const
	{
		return m_lZoom;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtWSMap::Sync2TileLayers()
	{
		vector<SmtTileWrapper*>::iterator iter = m_vTileWrappers.begin() ;

		while (iter != m_vTileWrappers.end())
		{
			(*iter)->Sync2TileLayer();
			++iter;
		}

		return SMT_ERR_NONE;
	}
}