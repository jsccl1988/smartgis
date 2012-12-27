#include "sde_mem.h"
#include "smt_api.h"
#include "geo_geometry.h"
#include "gis_feature.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEMem
{
	SmtMemVecLayer::SmtMemVecLayer(SmtDataSource *pOwnerDs):SmtVectorLayer(pOwnerDs)
	{
		m_pOwnerDs = NULL;
		m_pFilterGeom = NULL;
		sprintf(m_szLayerName,"MemLayer");  
		m_bIsVisible  = true;
		m_nIteratorIndex = 0;
	}

	SmtMemVecLayer::~SmtMemVecLayer(void)
	{
		if (IsOpen())
			Close();
		 
		SMT_SAFE_DELETE(m_pFilterGeom);

		DeleteAll();

	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtMemVecLayer::Open(const char *szName) 
	{ 
		m_bOpen = true;
		strcpy(m_szLayerName,szName);

		return m_bOpen;
	}

	bool SmtMemVecLayer::Create(void)
	{
		return true;
	}

	bool SmtMemVecLayer::Close(void)
	{
		m_bOpen = false;
		return true;
	}

	bool SmtMemVecLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		return true;
	}

	void SmtMemVecLayer::MoveFirst(void) const
	{
		if (!IsOpen())
			return;

		m_nIteratorIndex = 0;
	}

	void SmtMemVecLayer::MoveNext(void) const
	{
		if (!IsOpen())
			return;

		if (m_nIteratorIndex < m_vSmtFeatures.size())
			m_nIteratorIndex++;
	}

	void SmtMemVecLayer::MoveLast(void) const
	{
		if (!IsOpen())
			return;

		m_nIteratorIndex = m_vSmtFeatures.size() -1 ;
	}

	void SmtMemVecLayer::Delete(void)
	{
		if (!IsOpen())
			return;

		int i = 0;
		vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
		while (iter != m_vSmtFeatures.end() && i < m_nIteratorIndex)
		{
			i++;
			iter++;
		}

		if (iter != m_vSmtFeatures.end())
		{
			SMT_SAFE_DELETE(*iter);
			m_vSmtFeatures.erase(iter);
		}

		CalEnvelope();
	}

	bool SmtMemVecLayer::IsEnd(void) const
	{
		return (m_nIteratorIndex == m_vSmtFeatures.size());
	}


	void SmtMemVecLayer::DeleteAll(void)
	{
		if (!IsOpen())
			return;

		vector<SmtFeature *>::iterator i = m_vSmtFeatures.begin() ;

		while (i != m_vSmtFeatures.end())
		{
			SMT_SAFE_DELETE(*i);
			++i;
		}
		m_vSmtFeatures.clear();
		m_nIteratorIndex = 0;
	}

	long  SmtMemVecLayer::Query(const SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer *pQueryResult)
	{
		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		if(pQueryResult)
		{
			vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
			while (iter != m_vSmtFeatures.end())
			{
				SmtGeometry *pGeom = (*iter)->GetGeometryRef();
				if (pGeom)
				{
					long lRs = pGQueryDesc->pQueryGeom->Relationship(pGeom,pGQueryDesc->fSmargin);

					if (pGQueryDesc->sSRs &lRs)
						pQueryResult->AppendFeature((*iter),true);
				}
				iter++;
			}
		}
		return SMT_ERR_NONE;
	}


	long SmtMemVecLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bclone)
	{
		/*
		vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
		while (iter != m_vSmtFeatures.end())
		{
		if (pSmtFeature->GetID() == (*iter)->GetID())
		return SMT_ERR_FAILURE;
		iter++;
		}
		*/

		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		m_vSmtFeatures.push_back((bclone) ? pSmtFeature->Clone():const_cast<SmtFeature *>(pSmtFeature));

		Envelope         oGeomEnv;
		const SmtGeometry *pSmtGeom = pSmtFeature->GetGeometryRef();
		if (pSmtGeom)
		{
			pSmtGeom->GetEnvelope( &oGeomEnv );
			m_lyrEnv.Merge(oGeomEnv);
		}
	
		return SMT_ERR_NONE;
	}

	long SmtMemVecLayer::AppendFeatureBatch(const SmtFeature * pSmtFeature,bool bClone)
	{
		return AppendFeature(pSmtFeature,bClone);
	}

	long  SmtMemVecLayer::UpdateFeature(const SmtFeature * pSmtFeature)
	{
		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
		while (iter != m_vSmtFeatures.end())
		{
			if (pSmtFeature == (*iter))
				return SMT_ERR_NONE;
			 
			if (pSmtFeature->GetID() == (*iter)->GetID())
			{
				SMT_SAFE_DELETE(*iter);
				(*iter) = pSmtFeature->Clone();

				return SMT_ERR_NONE;
			}
			iter++;
		}
		return SMT_ERR_FAILURE;
	}

	long  SmtMemVecLayer::DeleteFeature(const SmtFeature * pSmtFeature)
	{
		if (!IsOpen())
			return SMT_ERR_DB_OPER;

		vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
		while (iter != m_vSmtFeatures.end())
		{
			if (pSmtFeature->GetID() == (*iter)->GetID())
			{
				SMT_SAFE_DELETE(*iter);
				m_vSmtFeatures.erase(iter);
				return SMT_ERR_NONE;
			}
			iter++;
		}
		return SMT_ERR_FAILURE;
	}

	SmtFeature * SmtMemVecLayer::GetFeature() const 
	{
		if (!IsOpen())
			return NULL;

		return GetFeature(m_nIteratorIndex);
	}

	SmtFeature * SmtMemVecLayer::GetFeature(int index) const 
	{
		if (!IsOpen())
			return NULL;

		if (m_vSmtFeatures.size() < 1||index < 0 || index > (m_vSmtFeatures.size()-1) )
			return NULL;

		return m_vSmtFeatures[index];
	}

	SmtFeature * SmtMemVecLayer::GetFeatureByID(uint unID) const 
	{
		if (!IsOpen())
			return NULL;

		/*	vector<SmtFeature*>::iterator iter = m_vSmtFeatures.begin() ;
		while (iter != m_vSmtFeatures.end())
		{
			if (unID == (*iter)->GetID())
			{
				return (*iter);
			}
			iter++;
		}*/
		for (int i = 0; i < m_vSmtFeatures.size();i++)
		{
			if (NULL != m_vSmtFeatures[i] && 
				unID == (m_vSmtFeatures[i])->GetID())
			{
				return (m_vSmtFeatures[i]);
			}
		}

		return NULL;
	}

	void  SmtMemVecLayer::CalEnvelope(void)
	{
		if (!IsOpen())
			return;

		m_lyrEnv  = Envelope();

		Envelope         oGeomEnv;
		for( int iFeature = 0; iFeature < m_vSmtFeatures.size(); iFeature++ )
		{
			m_vSmtFeatures[iFeature]->GetGeometryRef()->GetEnvelope( &oGeomEnv );
			m_lyrEnv.Merge(oGeomEnv);	 
		}
	}
}