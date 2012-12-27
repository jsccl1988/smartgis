#include "sde_ado.h"
#include "sde_mem.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtAdoVecLayer::SmtAdoVecLayer(SmtDataSource *pOwnerDs):SmtVectorLayer(pOwnerDs)
		,m_pGeomFieldCollect(NULL)
		,m_pTableFieldCollect(NULL)
	{
		m_pOwnerDs = pOwnerDs->Clone();
		m_pOwnerDs->Open();


		m_pFilterGeom = NULL; 
		m_bIsVisible  = true;

		m_SmtRecordset.SetAdoConnection(&(((SmtAdoDataSource*)m_pOwnerDs)->GetConnection()));

		SmtDataSource *pDS = new SmtMemDataSource();
		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			m_pMemLayer = pDS->CreateVectorLayer("SmtMemVecLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);
	}

	SmtAdoVecLayer::~SmtAdoVecLayer(void)
	{
		//////////////////////////////////////////////////////////////////////////
		SMT_SAFE_DELETE(m_pGeomFieldCollect);
		SMT_SAFE_DELETE(m_pTableFieldCollect);

		//////////////////////////////////////////////////////////////////////////
		//db
		if (IsOpen())
		{
			Close();
		}

		//////////////////////////////////////////////////////////////////////////
		//mem
		 SMT_SAFE_DELETE(m_pMemLayer);
		
		 //////////////////////////////////////////////////////////////////////////
	
		if(m_SmtRecordset.IsOpen())
			m_SmtRecordset.Close();

		if (m_pOwnerDs->IsOpen())
		{
			m_pOwnerDs->Close();
			SMT_SAFE_DELETE(m_pOwnerDs);
		}  
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtAdoVecLayer::Open(const char *szLayerTableName)
	{
		sprintf(m_szLayerTableName,szLayerTableName);

		if(m_SmtRecordset.IsOpen())
			m_SmtRecordset.Close();

		m_bOpen =  ((SmtAdoDataSource *)m_pOwnerDs)->OpenTable(m_szLayerTableName,&m_SmtRecordset) && m_pMemLayer->Open(m_szLayerTableName);
	
		return m_bOpen;
	}

	bool SmtAdoVecLayer::Close(void)
	{
		m_bOpen = !(((SmtAdoDataSource *)m_pOwnerDs)->CloseTable(m_szLayerTableName,&m_SmtRecordset) && m_pMemLayer->Close());
		return true;
	}

	bool SmtAdoVecLayer::Fetch(eSmtFetchType type)
	{
		if (m_SmtRecordset.IsOpen())
			m_SmtRecordset.Close();
	
		m_bOpen = ((SmtAdoDataSource *)m_pOwnerDs)->OpenTable(m_szLayerTableName,&m_SmtRecordset,adOpenForwardOnly);

		if (!IsOpen())
			return false;

		if (m_pAtt == NULL)
			SetDefaultAttFields();

		if (m_pGeomFieldCollect == NULL)
			SetDefaultGeomFields();

		if (m_pTableFieldCollect == NULL)
			SetTableFields();

		fRect lyrRect;
		SetLayerRect(lyrRect);

		m_SmtRecordset.MoveFirst();
		while(!m_SmtRecordset.IsEOF())
		{
			SmtFeature * pSmtFeature = new SmtFeature();
			GetFeature(pSmtFeature);
			m_pMemLayer->AppendFeature(pSmtFeature);
			m_SmtRecordset.MoveNext();
		}

		Open(m_szLayerTableName);

		CalEnvelope();

		return true;
	}

	long SmtAdoVecLayer::UpdateFeatureBatch(void)
	{
		if (!IsOpen())
			return SMT_ERR_DB_OPER;
		else
		{
			m_SmtRecordset.UpdateBatch();
			CalEnvelope();
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtAdoVecLayer::GetFeatureCount(void)  const
	{
       return m_pMemLayer->GetFeatureCount();
	}

	void SmtAdoVecLayer::MoveFirst(void) const
	{
		 m_pMemLayer->MoveFirst();
	}

	void SmtAdoVecLayer::MoveNext(void) const
	{
		m_pMemLayer->MoveNext();
	}

	void SmtAdoVecLayer::MoveLast(void) const
	{
		m_pMemLayer->MoveLast();
	}

	void SmtAdoVecLayer::Delete(void)
	{
		SmtFeature *pSmtFea = m_pMemLayer->GetFeature();
		if (NULL != pSmtFea)
		{
			SmtAdoRecordSet smtRecordset;
			smtRecordset.SetAdoConnection(&(((SmtAdoDataSource*)m_pOwnerDs)->GetConnection()));

			char szSQL[TEMP_BUFFER_SIZE];
			sprintf_s(szSQL,TEMP_BUFFER_SIZE,"select * from %s where ID = %d;",m_szLayerTableName,pSmtFea->GetID());
			if (smtRecordset.Open(szSQL))
			{
				smtRecordset.MoveFirst();
				if (smtRecordset.IsOpen() && !smtRecordset.IsEOF() && smtRecordset.GetRecordCount() == 1)
				{
					if (smtRecordset.Delete())
					{
						//mem
						m_pMemLayer->Delete();
						CalEnvelope();
					}
				}

				Open(m_szLayerTableName);
			}
		}
	}

	bool SmtAdoVecLayer::IsEnd(void) const
	{
		return m_pMemLayer->IsEnd();
	}

	void SmtAdoVecLayer::DeleteAll(void)
	{
		if (((SmtAdoDataSource* )m_pOwnerDs)->ClearTable(m_szLayerTableName))
		{
			//mem
			m_pMemLayer->DeleteAll();
			CalEnvelope();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	SmtFeature *SmtAdoVecLayer::GetFeature()  const 
	{
		return m_pMemLayer->GetFeature();
	}

	SmtFeature *SmtAdoVecLayer::GetFeature(int index) const 
	{
		return m_pMemLayer->GetFeature(index);
	}

	SmtFeature *SmtAdoVecLayer::GetFeatureByID(uint unID) const 
	{
		return m_pMemLayer->GetFeatureByID(unID);
	}

	long  SmtAdoVecLayer::DeleteFeature(const SmtFeature *pSmtFeature)
	{		
		if ( NULL == pSmtFeature)
			return SMT_ERR_INVALID_PARAM;

		char szSQL[TEMP_BUFFER_SIZE];
		sprintf_s(szSQL,TEMP_BUFFER_SIZE,"select * from %s where ID = %d;",m_szLayerTableName,pSmtFeature->GetID());
		if (m_SmtRecordset.Open(szSQL))
		{
			m_SmtRecordset.MoveFirst();
			if (m_SmtRecordset.IsOpen() && !m_SmtRecordset.IsEOF() && m_SmtRecordset.GetRecordCount() == 1)
			{
				if (m_SmtRecordset.Delete())
				{
					//mem
					m_pMemLayer->DeleteFeature(pSmtFeature);
					CalEnvelope();
				}
			}
		}

		return SMT_ERR_NONE;
	}

	long  SmtAdoVecLayer::Query(const SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer *pQueryResult)
	{
		return m_pMemLayer->Query(pGQueryDesc,pPQueryDesc,pQueryResult);
	}

	void  SmtAdoVecLayer::CalEnvelope(void)
	{
         Envelope env;
		 m_pMemLayer->CalEnvelope();
		 m_pMemLayer->GetEnvelope(env);
		 memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	}

	void  SmtAdoVecLayer::SetLayerRect(const fRect &lyrRect)
	{
		m_pMemLayer->SetLayerRect(lyrRect);
		memcpy(&m_lyrEnv,&lyrRect,sizeof(fRect));
	}
}