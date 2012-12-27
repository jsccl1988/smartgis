#include "sde_mem.h"
#include "smt_api.h"
#include "geo_geometry.h"
#include "gis_feature.h"
#include "bl_api.h"

using namespace Smt_Base;
using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEMem
{
	SmtMemTileLayer::SmtMemTileLayer(SmtDataSource *pOwnerDs):SmtTileLayer(pOwnerDs)
	{
		m_pOwnerDs = NULL;
		sprintf(m_szLayerName,"MemLayer");  
		m_bIsVisible  = true;
		m_nIteratorIndex = 0;
	}

	SmtMemTileLayer::~SmtMemTileLayer(void)
	{
		if (IsOpen())
			Close();
		 
		DeleteAll();

	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtMemTileLayer::Open(const char *szName) 
	{ 
		m_bOpen = true;
		strcpy(m_szLayerName,szName);

		return m_bOpen;
	}

	bool SmtMemTileLayer::Create(void)
	{
		return true;
	}

	bool SmtMemTileLayer::Close(void)
	{
		m_bOpen = false;
		return true;
	}

	bool SmtMemTileLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		return true;
	}

	void SmtMemTileLayer::MoveFirst(void) const
	{
		if (!IsOpen())
			return;

		m_nIteratorIndex = 0;
	}

	void SmtMemTileLayer::MoveNext(void) const
	{
		if (!IsOpen())
			return;

		if (m_nIteratorIndex < m_vTilePtrs.size())
			m_nIteratorIndex++;
	}

	void SmtMemTileLayer::MoveLast(void) const
	{
		if (!IsOpen())
			return;

		m_nIteratorIndex = m_vTilePtrs.size() -1 ;
	}

	void SmtMemTileLayer::Delete(void)
	{
		if (!IsOpen())
			return;

		int i = 0;
		vector<SmtTile*>::iterator iter = m_vTilePtrs.begin() ;
		while (iter != m_vTilePtrs.end() && i < m_nIteratorIndex)
		{
			i++;
			iter++;
		}

		if (iter != m_vTilePtrs.end())
		{
			SMT_SAFE_DELETE_A((*iter)->pTileBuf);
			m_vTilePtrs.erase(iter);
		}

		CalEnvelope();
	}

	bool SmtMemTileLayer::IsEnd(void) const
	{
		return (m_nIteratorIndex == m_vTilePtrs.size());
	}

	void SmtMemTileLayer::DeleteAll(void)
	{
		if (!IsOpen())
			return;

		vector<SmtTile *>::iterator iter = m_vTilePtrs.begin() ;

		while (iter != m_vTilePtrs.end())
		{
			SMT_SAFE_DELETE_A((*iter)->pTileBuf);
			++iter;
		}

		m_vTilePtrs.clear();
		m_nIteratorIndex = 0;
	}

	long SmtMemTileLayer::AppendTile(const SmtTile *pTile,bool bClone )
	{
		if (NULL == pTile || NULL == pTile->pTileBuf)
			return SMT_ERR_INVALID_PARAM;

		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		if (NULL != GetTileByID(pTile->lID))
			return SMT_ERR_FAILURE;
		 
		if (bClone)
		{
			SmtTile *pNewTile = new SmtTile;
			pNewTile->pTileBuf = new char[pTile->lTileBufSize];
			memcpy(pNewTile->pTileBuf,pTile->pTileBuf,sizeof(char)*pTile->lTileBufSize);

			pNewTile->lID = pTile->lID;
			pNewTile->lImageCode = pTile->lImageCode;
			pNewTile->lTileBufSize = pTile->lTileBufSize;
			pNewTile->rtTileRect = pTile->rtTileRect;
			pNewTile->bVisible		= pTile->bVisible;

			m_vTilePtrs.push_back(pNewTile);

			Envelope         titleEnv;
			RectToEnvelope(titleEnv,pNewTile->rtTileRect);
			m_lyrEnv.Merge(titleEnv);
		}
		else
		{
			m_vTilePtrs.push_back(const_cast<SmtTile *>(pTile));

			Envelope         titleEnv;
			RectToEnvelope(titleEnv,pTile->rtTileRect);
			m_lyrEnv.Merge(titleEnv);
		}

		return SMT_ERR_NONE;
	}

	long SmtMemTileLayer::UpdateTile(const SmtTile *pTile)
	{
		if (NULL == pTile)
			return SMT_ERR_INVALID_PARAM;

		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		SmtTile *pOldTile = GetTileByID(pTile->lID);

		if (NULL == pOldTile)
		{
			m_vTilePtrs.push_back(const_cast<SmtTile *>(pTile));

			Envelope         titleEnv;
			RectToEnvelope(titleEnv,pTile->rtTileRect);
			m_lyrEnv.Merge(titleEnv);

			return SMT_ERR_NONE;
		}
		else
		{
			SMT_SAFE_DELETE_A(pOldTile->pTileBuf);
			pOldTile->pTileBuf = new char[pTile->lTileBufSize];
			memcpy(pOldTile->pTileBuf,pTile->pTileBuf,sizeof(char)*pTile->lTileBufSize);

			pOldTile->lID = pTile->lID;
			pOldTile->lImageCode = pTile->lImageCode;
			pOldTile->lTileBufSize = pTile->lTileBufSize;
			pOldTile->rtTileRect = pTile->rtTileRect;
			pOldTile->bVisible		= pTile->bVisible;
		}

		vector<SmtTile*>::iterator iter = m_vTilePtrs.begin() ;
		while (iter != m_vTilePtrs.end())
		{
			if (pTile->lID == (*iter)->lID)
			{
				return SMT_ERR_NONE;
			}
			iter++;
		}
		return SMT_ERR_FAILURE;
	}

	long SmtMemTileLayer::DeleteTile(const SmtTile *pTile)
	{
		if (NULL == pTile)
			return SMT_ERR_INVALID_PARAM;
	 
		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		vector<SmtTile*>::iterator iter = m_vTilePtrs.begin() ;
		while (iter != m_vTilePtrs.end())
		{
			if (pTile->lID == (*iter)->lID)
			{
				SMT_SAFE_DELETE_A((*iter)->pTileBuf);
				m_vTilePtrs.erase(iter);
				return SMT_ERR_NONE;
			}
			iter++;
		}
		return SMT_ERR_FAILURE;
	}

	SmtTile* SmtMemTileLayer::GetTile()  const 
	{
		if (!IsOpen())
			return NULL;

		return GetTile(m_nIteratorIndex);
	}

	SmtTile* SmtMemTileLayer::GetTile(int index)  const 
	{
		if (!IsOpen())
			return NULL;

		if (m_vTilePtrs.size() < 1||index < 0 || index > (m_vTilePtrs.size()-1) )
			return NULL;

		return m_vTilePtrs[index];
	}

	SmtTile* SmtMemTileLayer::GetTileByID(uint unID)  const
	{
		if (!IsOpen())
			return NULL;

		for (int i = 0; i < m_vTilePtrs.size();i++)
		{
			if (NULL != m_vTilePtrs[i] && 
				unID == (m_vTilePtrs[i])->lID)
			{
				return (m_vTilePtrs[i]);
			}
		}

		return NULL;
	}

	void  SmtMemTileLayer::CalEnvelope(void)
	{
		if (!IsOpen())
			return;

		m_lyrEnv  = Envelope();

		Envelope         titleEnv;
		for( int iTile = 0; iTile < m_vTilePtrs.size(); iTile++ )
		{
			RectToEnvelope(titleEnv,m_vTilePtrs[iTile]->rtTileRect);
			m_lyrEnv.Merge(titleEnv);	 
		}
	}
}