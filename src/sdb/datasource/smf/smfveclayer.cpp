#include "sdb/datasource/smf/smf.h"
#include "base/core/api.h"
#include "sdb/feature/feature_api.h"
#include "sdb/datasource/mem/mem.h"
#include "sdb/datasource/smf/smf_ogrsupport.h"

using namespace geo;
using namespace base;
using namespace sdb;

namespace sdb
{
	SmtSmfVecLayer::SmtSmfVecLayer(SmtDataSource *pOwnerDs):SmtVectorLayer(pOwnerDs)
	{
		m_pOwnerDs = pOwnerDs->clone();
		m_pFilterGeom = NULL;
		sprintf(m_szLayerName,"DefShape");  
		m_pAtt = new SmtAttribute();

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

	SmtSmfVecLayer::~SmtSmfVecLayer(void)
	{
       SMT_SAFE_DELETE(m_pFilterGeom);

	   SMT_SAFE_DELETE(m_pOwnerDs);

	   //////////////////////////////////////////////////////////////////////////
	   //mem
	    SMT_SAFE_DELETE(m_pMemLayer);
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtSmfVecLayer::Create(void)
	{
		 sprintf(m_szLayerFileName,m_szLayerName);
		 m_pMemLayer->Create();

         return true;
	}

	bool SmtSmfVecLayer::Open(const char * szLayerFileName)
	{
		sprintf(m_szLayerFileName,szLayerFileName);

		m_bOpen = m_pMemLayer->Open(m_szLayerFileName);
		
		return m_bOpen;
	}

	bool SmtSmfVecLayer::Close(void)
	{
		m_bOpen = m_pMemLayer->Close();
		return m_bOpen;
	}

	bool SmtSmfVecLayer::Fetch(eSmtFetchType type)
	{
		if (!IsOpen())
			return false;

		char szShpUrl[MAX_LAYER_FILE_NAME];
		
		SmtDataSourceInfo info;
		m_pOwnerDs->GetInfo(info);

		sprintf(szShpUrl,"%s.shp",m_szLayerFileName);

		fRect lyrRect;
		SetLayerRect(lyrRect);
		
		return PreReadShp(szShpUrl) && ReadShp(szShpUrl);
	}

	void SmtSmfVecLayer::MoveFirst(void) const
	{
		m_pMemLayer->MoveFirst();
	}

	void SmtSmfVecLayer::MoveNext(void) const
	{
		m_pMemLayer->MoveNext();
	}

	void SmtSmfVecLayer::MoveLast(void) const
	{
       m_pMemLayer->MoveLast();
	}

	void SmtSmfVecLayer::Delete(void)
	{
		m_pMemLayer->Delete();
		
		m_pMemLayer->CalEnvelope();
	}

	bool SmtSmfVecLayer::IsEnd(void) const
	{
		return (m_pMemLayer->IsEnd());
	}

	void SmtSmfVecLayer::DeleteAll(void)
	{
		m_pMemLayer->DeleteAll();
	}

	long  SmtSmfVecLayer::Query(CONST SmtGQueryDesc *pGQueryDesc,const SmtPQueryDesc *pPQueryDesc,SmtVectorLayer * pQueryResult)
	{
		return m_pMemLayer->Query(pGQueryDesc,pPQueryDesc,pQueryResult);
	}

	long SmtSmfVecLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bClone)
	{
		m_pMemLayer->AppendFeature(pSmtFeature,bClone);

		Envelope env;
		// m_pMemLayer->CalEnvelope();
		m_pMemLayer->get_envelope(env);
		memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	
		return SMT_ERR_NONE;
	}

	long  SmtSmfVecLayer::UpdateFeature(const SmtFeature * pSmtFeature) 
	{
		return m_pMemLayer->UpdateFeature(pSmtFeature);
	}

	long  SmtSmfVecLayer::DeleteFeature(const SmtFeature * pSmtFeature)
	{
		return m_pMemLayer->DeleteFeature(pSmtFeature);
	}

	SmtFeature *SmtSmfVecLayer::GetFeature() const 
	{
		return m_pMemLayer->GetFeature();
	}

	SmtFeature *SmtSmfVecLayer::GetFeature(int index) const 
	{
		return m_pMemLayer->GetFeature(index);
	}

	SmtFeature *SmtSmfVecLayer::GetFeatureByID(uint unID) const 
	{
		return m_pMemLayer->GetFeatureByID(unID);
	}

	void  SmtSmfVecLayer::CalEnvelope(void)
	{
		Envelope env;
		m_pMemLayer->CalEnvelope();
		m_pMemLayer->get_envelope(env);
		memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	}

	void  SmtSmfVecLayer::SetLayerRect(const fRect &lyrRect)
	{
		m_pMemLayer->SetLayerRect(lyrRect);
		memcpy(&m_lyrEnv,&lyrRect,sizeof(fRect));
	}

	//////////////////////////////////////////////////////////////////////////
	static OGRDataSource *OpenShpReadOnly(const char *szShpName)
	{
		GDALDataset *poDS = static_cast<GDALDataset *>(
			GDALOpenEx(szShpName, GDAL_OF_VECTOR, NULL, NULL, NULL));
		return static_cast<OGRDataSource *>(poDS);
	}

	bool SmtSmfVecLayer::PreReadShp(const char * szShpName)
	{
		OGRDataSource       *poDS;

		poDS = OpenShpReadOnly(szShpName);

		if( poDS == NULL )
			return false;

		OGRLayer  *poLayer = NULL;
		OGRFeature *poFeature = NULL;

		poLayer = poDS->GetLayer(0);
		poLayer->ResetReading();
		poFeature = poLayer->GetNextFeature();
		if (NULL != poFeature)
		{
			long type = SmtUnknown;
			OGRGeometry *poGeometry;
			poGeometry = poFeature->GetGeometryRef();		
			if( poGeometry != NULL )
				ogr_fea_type_to_smt_fea_type(wkbFlatten(poGeometry->getGeometryType()),type);

			m_SmtLayerFtType = (SmtFeatureType)type;
			
			//////////////////////////////////////////////////////////////////////////
			SmtField smtFld;
			OGRFeatureDefn *poDefn = poFeature->GetDefnRef();
			if (NULL != poDefn)
			{
				int iCount = poDefn->GetFieldCount();
				for (int i = 0; i < iCount; i++)
				{
					OGRFieldDefn * pFldDefn = poDefn->GetFieldDefn(i);
					if (NULL != pFldDefn)
					{
						long fldtype = SmtUnknown;	 
						ogr_fld_type_to_smt_fld_type(pFldDefn->GetType(),fldtype);
						smtFld.SetName(pFldDefn->GetNameRef());
						smtFld.SetType(fldtype);
						m_pAtt->AddField(smtFld);
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		OGRFeature::DestroyFeature( poFeature );
		OGRDataSource::DestroyDataSource( poDS );

		return true;
	}

	bool  SmtSmfVecLayer::ReadShp(const char *szShpName)
	{
		OGRDataSource       *poDS;

		poDS = OpenShpReadOnly(szShpName);
		if( poDS == NULL )
			 return false;
	
		OGRLayer  *poLayer;
		poLayer = poDS->GetLayer(0);
		OGRFeature *poFeature;
		int nFeaID = 0;

		poLayer->ResetReading();
		while( (poFeature = poLayer->GetNextFeature()) != NULL)// && nPointID < MAX_FEATURE && nCurveID < MAX_FEATURE  && nSurfaceID < MAX_FEATURE)
		{
			SmtFeature *pSmtFeature = new SmtFeature;

			pSmtFeature->SetID(nFeaID);
			pSmtFeature->SetAttribute(m_pAtt);
		
			if (copy_ogr_fea_to_smt_fea(poFeature,pSmtFeature))
			{
				AppendFeature(pSmtFeature,false);
				nFeaID ++;
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);
		
			OGRFeature::DestroyFeature( poFeature );
		}

		OGRDataSource::DestroyDataSource( poDS );

		return true;
		//
	/*	if (SmtFtDot == m_SmtLayerFtType)
		{
			points_to_multi_point(this);
		}*/

		return true;
	}
}