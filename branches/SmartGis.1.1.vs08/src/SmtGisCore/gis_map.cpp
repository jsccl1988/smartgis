#include "gis_map.h"
#include "gis_api.h"
#include "smt_api.h"
using namespace Smt_Geo;
using namespace Smt_Core;

namespace Smt_GIS
{
	SmtMap::SmtMap(void)
	{
		strcpy_s(m_szMapName,MAX_MAP_NAME,"DefMap");
        m_pCurrentLayer = NULL;
		m_MapEnvelope.MinX = 0;
		m_MapEnvelope.MinY = 0;
		m_MapEnvelope.MaxX = 800;
		m_MapEnvelope.MaxY = 600;
	}

	SmtMap::~SmtMap(void)
	{
		vector<SmtLayer*>::iterator iter = m_vSmtLayers.begin() ;

		while (iter != m_vSmtLayers.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}
		m_vSmtLayers.clear();

		m_pCurrentLayer = NULL;
	}

	bool SmtMap::AddLayer(const SmtLayer * pLayer)
	{
		bool bRet = false;
        if (GetLayer(pLayer->GetLayerName()) == NULL)
        {
			if (m_vSmtLayers.size() == 0)
				m_MapEnvelope.MaxX = m_MapEnvelope.MinX = m_MapEnvelope.MaxY = m_MapEnvelope.MinY = SMT_C_INVALID_DBF_VALUE;

			m_vSmtLayers.push_back(const_cast<SmtLayer *>(pLayer));

			m_pCurrentLayer = m_vSmtLayers[m_vSmtLayers.size()-1];

			Envelope         oLyrEnv;
			pLayer->GetEnvelope(oLyrEnv);
			m_MapEnvelope.Merge(oLyrEnv);

			bRet = true;
        }

		return bRet;
	}

	bool SmtMap::DeleteLayer(const char * szName)
	{
		bool bRet = true;
		vector<SmtLayer*>::iterator iter = m_vSmtLayers.begin() ;
		while (iter != m_vSmtLayers.end())
		{
			if (strcmp((*iter)->GetLayerName(),szName) == 0)
			{
				if ((*iter) == m_pCurrentLayer)
					m_pCurrentLayer = NULL;
				 
               SMT_SAFE_DELETE(*iter);
			   break;
			}
			iter++;
		}

		if (iter != m_vSmtLayers.end())
		{
			m_vSmtLayers.erase(iter);

			bRet = true;

			CalEnvelope();
		}

		return bRet;
	}

	bool SmtMap::DeleteLayer(const SmtLayer *pLayer)
	{
		return DeleteLayer(pLayer->GetLayerName());
	}

	bool SmtMap::MoveTo (int fromIndex, int toIndex)
	{
		SmtLayer *pSmtLayer;
		pSmtLayer = m_vSmtLayers[fromIndex];
		m_vSmtLayers[fromIndex] = m_vSmtLayers[toIndex];
		m_vSmtLayers[toIndex] = pSmtLayer;

		return true;
	}

	bool SmtMap::MoveToBottom (int index)
	{
		SmtLayer *pSmtLayer;
		int nLast = m_vSmtLayers.size() - 1;
		pSmtLayer = m_vSmtLayers[nLast] ;
		m_vSmtLayers[nLast] = m_vSmtLayers[index];
		m_vSmtLayers[index] = pSmtLayer;

		return true;
	}

	bool SmtMap::MoveToTop (int index)
	{
		SmtLayer *pSmtLayer;
		pSmtLayer = m_vSmtLayers[0] ;
		m_vSmtLayers[0] = m_vSmtLayers[index];
		m_vSmtLayers[index] = pSmtLayer;

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	SmtLayer* SmtMap::GetLayer(const char * szName)
	{
        SmtLayer* pLayer = NULL;
		vector<SmtLayer*>::iterator i = m_vSmtLayers.begin() ;

		while (i != m_vSmtLayers.end())
		{
			if (strcmp((*i)->GetLayerName(),szName) == 0)
			{
				pLayer =  (*i);
				break;
			}
			++i;
		}

		return pLayer;
	}

	const SmtLayer* SmtMap::GetLayer(const char * szName) const
	{
		SmtLayer* pLayer = NULL;
		vector<SmtLayer*>::const_iterator i = m_vSmtLayers.begin() ;

		while (i != m_vSmtLayers.end())
		{
			if (strcmp((*i)->GetLayerName(),szName) == 0)
			{
				pLayer =  (*i);
				break;
			}
			++i;
		}

		return pLayer;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtMap::MoveFirst(void) const 
	{
		m_nIteratorIndex = 0;
	}

	void SmtMap::MoveNext(void) const 
	{
		if (m_nIteratorIndex < m_vSmtLayers.size())
			m_nIteratorIndex++;
	}

	void SmtMap::MoveLast(void) const 
	{
		m_nIteratorIndex = m_vSmtLayers.size() -1 ;
	}

	void SmtMap::Delete(void)
	{
		SmtLayer *pSmtLayer = m_vSmtLayers[m_nIteratorIndex];

		m_vSmtLayers.erase( m_vSmtLayers.begin() + m_nIteratorIndex );

		SMT_SAFE_DELETE(pSmtLayer);

		CalEnvelope();
	}

	void SmtMap::DeleteAll(void)
	{
		vector<SmtLayer*>::iterator iter = m_vSmtLayers.begin() ;

		while (iter != m_vSmtLayers.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}
		m_vSmtLayers.clear();

		m_pCurrentLayer = NULL;
	}

	bool SmtMap::IsEnd(void)  const 
	{
		return (m_nIteratorIndex == m_vSmtLayers.size());
	}

	SmtLayer * SmtMap::GetLayer()
	{
		return GetLayer(m_nIteratorIndex);
	}

	const SmtLayer * SmtMap::GetLayer()   const 
	{
		return GetLayer(m_nIteratorIndex);
	}

	SmtLayer * SmtMap::GetLayer(int index)
	{
		if (index < 0 || index > (m_vSmtLayers.size()-1) )
			return NULL;

		return m_vSmtLayers[index];
	}

	const SmtLayer * SmtMap::GetLayer(int index)  const 
	{
		if (index < 0 || index > (m_vSmtLayers.size()-1) )
			return NULL;
		return m_vSmtLayers[index];
	}

	void  SmtMap::CalEnvelope(void)
	{
		Envelope         oLyrEnv;
		m_MapEnvelope = oLyrEnv;

		for( int iLayer = 0; iLayer < m_vSmtLayers.size(); iLayer++ )
		{
			m_vSmtLayers[iLayer]->CalEnvelope();
			m_vSmtLayers[iLayer]->GetEnvelope(oLyrEnv);
			m_MapEnvelope.Merge(oLyrEnv);	  
		}
	}

	bool SmtMap::AppendFeature(const SmtFeature *pC_Feature,bool bIsClone)
	{
		if (NULL == pC_Feature)
			return false;
		 
		bool bRet = false;

		SmtFeature *pFeature = const_cast<SmtFeature *>(pC_Feature);

		if (m_pCurrentLayer)
		{
			if (LYR_VECTOR == m_pCurrentLayer->GetLayerType())
			{
				SmtVectorLayer *pVLayer = (SmtVectorLayer *)m_pCurrentLayer;
				int				nID = pVLayer->GetFeatureCount()+1;	

				pFeature->SetID(nID);

				if (pFeature->GetFeatureType() == pVLayer->GetLayerFeatureType())
				{
					bRet = (SMT_ERR_NONE == pVLayer->AppendFeature(pFeature,bIsClone));
				}		
				else
					MessageBeep(0xFFFFFFFF);
			}
			else
				MessageBeep(0xFFFFFFFF);
		}		

		return bRet;
	}

	bool SmtMap::DeleteFeature(const SmtFeature *pFeature)
	{
		if (NULL == pFeature)
			return false;

		bool bRet = false;
		if (m_pCurrentLayer)
		{
			if (LYR_VECTOR == m_pCurrentLayer->GetLayerType())
			{
				SmtVectorLayer *pVLayer = (SmtVectorLayer *)m_pCurrentLayer;
		
				if (pFeature->GetFeatureType() == pVLayer->GetLayerFeatureType())
				{
					bRet = (SMT_ERR_NONE == pVLayer->DeleteFeature(pFeature));
				}		
				else
					MessageBeep(0xFFFFFFFF);
			}
			else
				MessageBeep(0xFFFFFFFF);
		}		

		return bRet;
	}

	bool SmtMap::UpdateFeature(const SmtFeature *pFeature)
	{
		if (NULL == pFeature)
			return false;

		bool bRet = false;

		if (m_pCurrentLayer)
		{
			if (LYR_VECTOR == m_pCurrentLayer->GetLayerType())
			{
				SmtVectorLayer *pVLayer = (SmtVectorLayer *)m_pCurrentLayer;
		
				if (pFeature->GetFeatureType() == pVLayer->GetLayerFeatureType())
				{
					bRet = (SMT_ERR_NONE == pVLayer->UpdateFeature(pFeature));
				}		
				else
					MessageBeep(0xFFFFFFFF);
			}
			else
				MessageBeep(0xFFFFFFFF);
		}		

		return bRet;
	}


	bool SmtMap::QueryFeature(const SmtGQueryDesc *  pC_GQueryDesc,const SmtPQueryDesc * pC_PQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType)
	{
		bool bRet = false;

		SmtGQueryDesc *pGQueryDesc = const_cast<SmtGQueryDesc *>(pC_GQueryDesc);
		SmtPQueryDesc *pPQueryDesc = const_cast<SmtPQueryDesc *>(pC_PQueryDesc);

		if (m_pCurrentLayer)
		{
			if (LYR_VECTOR == m_pCurrentLayer->GetLayerType())
			{
				SmtVectorLayer *pVLayer = (SmtVectorLayer *)m_pCurrentLayer;
				nFeaType = pVLayer->GetLayerFeatureType();
				pGQueryDesc->sSRs = (SmtSpatialRs)GetQueryRs(pGQueryDesc->pQueryGeom->GetGeometryType(),nFeaType);
				bRet = (pVLayer->Query(pGQueryDesc,pPQueryDesc,pQueryResult) == SMT_ERR_NONE);
			}
		}		

		return bRet;
	}
}