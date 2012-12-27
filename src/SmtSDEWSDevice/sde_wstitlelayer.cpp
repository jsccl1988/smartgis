#include "sde_ws.h"
#include "smt_api.h"
#include "gis_api.h"
#include "bl_api.h"
#include "ximage.h"
#include "sde_mem.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

namespace Smt_SDEWS
{
	SmtWSTileLayer::SmtWSTileLayer(SmtDataSource *pOwnerDs):SmtTileLayer(pOwnerDs)
	{
		m_pOwnerDs = pOwnerDs->Clone();

		sprintf(m_szLayerURL,"DefTileLayer");  
		m_pAtt = new SmtAttribute();

		SmtDataSource *pDS = new SmtMemDataSource();
		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			m_pMemLayer = pDS->CreateTileLayer("SmtWSTileLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);
	}

	SmtWSTileLayer::~SmtWSTileLayer(void)
	{
		SMT_SAFE_DELETE(m_pOwnerDs);
		if (IsOpen())
			Close();

		//////////////////////////////////////////////////////////////////////////
		//mem
		SMT_SAFE_DELETE(m_pMemLayer);
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtWSTileLayer::Open(const char *szLayerURL) 
	{ 
		sprintf(m_szLayerURL,szLayerURL);

		m_bOpen = m_pMemLayer->Open(szLayerURL);

		return m_bOpen;
	}

	bool SmtWSTileLayer::Create(void)
	{
		sprintf(m_szLayerURL,m_szLayerName);
		m_pMemLayer->Create();

		return true;
	}

	bool SmtWSTileLayer::Close(void)
	{
		m_bOpen = m_pMemLayer->Close();

		return m_bOpen;
	}

	bool SmtWSTileLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		return true;
	}
	
	void SmtWSTileLayer::MoveFirst(void) const
	{
		m_pMemLayer->MoveFirst();
	}

	void SmtWSTileLayer::MoveNext(void) const
	{
		m_pMemLayer->MoveNext();
	}

	void SmtWSTileLayer::MoveLast(void) const
	{
		m_pMemLayer->MoveLast();
	}

	void SmtWSTileLayer::Delete(void)
	{
		m_pMemLayer->Delete();

		m_pMemLayer->CalEnvelope();
	}

	bool SmtWSTileLayer::IsEnd(void) const
	{
		return (m_pMemLayer->IsEnd());
	}

	void SmtWSTileLayer::DeleteAll(void)
	{
		m_pMemLayer->DeleteAll();
	}

	long SmtWSTileLayer::AppendTile(const SmtTile *pTile,bool bClone )
	{
		return m_pMemLayer->AppendTile(pTile,bClone);
	}

	long SmtWSTileLayer::UpdateTile(const SmtTile *pTile)
	{
		return m_pMemLayer->UpdateTile(pTile);
	}

	long SmtWSTileLayer::DeleteTile(const SmtTile *pTile)
	{
		return m_pMemLayer->DeleteTile(pTile);
	}

	SmtTile* SmtWSTileLayer::GetTile()  const 
	{
		return m_pMemLayer->GetTile();
	}

	SmtTile* SmtWSTileLayer::GetTile(int index)  const 
	{
		return m_pMemLayer->GetTile(index);
	}

	SmtTile* SmtWSTileLayer::GetTileByID(uint unID)  const
	{
		return m_pMemLayer->GetTileByID(unID);
	}

	void  SmtWSTileLayer::CalEnvelope(void)
	{
		Envelope env;
		m_pMemLayer->CalEnvelope();
		m_pMemLayer->GetEnvelope(env);
		memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	}

	void  SmtWSTileLayer::SetLayerRect(const fRect &lyrRect)
	{
		m_pMemLayer->SetLayerRect(lyrRect);
		memcpy(&m_lyrEnv,&lyrRect,sizeof(fRect));
	}

}