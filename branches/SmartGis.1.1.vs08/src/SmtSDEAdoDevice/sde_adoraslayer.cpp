#include "sde_ado.h"
#include "sde_mem.h"
#include "bl_api.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtAdoRasLayer::SmtAdoRasLayer(SmtDataSource *pOwnerDs):SmtRasterLayer(pOwnerDs)
		,m_pGeomFieldCollect(NULL)
		,m_pTableFieldCollect(NULL)
	{
		m_pOwnerDs = pOwnerDs->Clone();
		m_pOwnerDs->Open();

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

			m_pMemLayer = pDS->CreateRasterLayer("SmtMemRasLayer",lyrRect,-1);
		}

		SMT_SAFE_DELETE(pDS);
	}

	SmtAdoRasLayer::~SmtAdoRasLayer(void)
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
	bool SmtAdoRasLayer::Open(const char *szLayerTableName)
	{
		sprintf(m_szLayerTableName,szLayerTableName);

		if(m_SmtRecordset.IsOpen())
			m_SmtRecordset.Close();

		m_bOpen =  ((SmtAdoDataSource *)m_pOwnerDs)->OpenTable(m_szLayerTableName,&m_SmtRecordset) && m_pMemLayer->Open(m_szLayerTableName);

		return m_bOpen;
	}

	bool SmtAdoRasLayer::Close(void)
	{
		m_bOpen = !(((SmtAdoDataSource *)m_pOwnerDs)->CloseTable(m_szLayerTableName,&m_SmtRecordset) && m_pMemLayer->Close());
		return true;
	}

	bool SmtAdoRasLayer::Fetch(eSmtFetchType type)
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
			SyncRead();
			m_SmtRecordset.MoveNext();
		}
		
		Open(m_szLayerTableName);
	
		CalEnvelope();

		return true;
	}

	bool SmtAdoRasLayer::Create(void)
	{
		bool bRet = true;

		//
		SetDefaultAttFields();
		SetDefaultGeomFields();
		SetTableFields();

		sprintf(m_szLayerTableName,"%s_RasFcls",m_szLayerName);
		SmtLayerInfo info;
		sprintf(info.szArchiveName,m_szLayerTableName);
		sprintf(info.szName,m_szLayerName);
		info.unFeatureType = SmtLayer_Ras;
		EnvelopeToRect(info.lyrRect,m_lyrEnv);
		
		return ((SmtAdoDataSource *)m_pOwnerDs)->CreateLayerTable(info,m_pTableFieldCollect);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtAdoRasLayer::SetDefaultAttFields(void)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		/*SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);*/
	}

	void SmtAdoRasLayer::SetDefaultGeomFields(void)
	{
		SMT_SAFE_DELETE(m_pGeomFieldCollect);
		m_pGeomFieldCollect = new SmtFieldCollect();

		SmtField smtFld;

		//raster
		smtFld.SetName("mbr_xmin");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_ymin");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_xmax");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_ymax");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("image_code");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("image_size");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("image_bin");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);
	}

	void SmtAdoRasLayer::SetTableFields(void)
	{
		SMT_SAFE_DELETE(m_pTableFieldCollect);
		m_pTableFieldCollect = new SmtFieldCollect();

		SmtField smtFld;
		//////////////////////////////////////////////////////////////////////////
		//raster
		smtFld.SetName("mbr_xmin");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_ymin");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_xmax");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("mbr_ymax");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("image_code");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("image_size");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("image_bin");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
		//////////////////////////////////////////////////////////////////////////
	}

	long SmtAdoRasLayer::SyncWrite(void)
	{
		long	 lImageCode = -1;
		char	*pRasterBuf = NULL;
		long	 lRasterBufSize = 0;
		fRect	 fRasterRect;

		m_pMemLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);

		m_SmtRecordset.AddNew();
		m_SmtRecordset.PutCollect("mbr_xmin",_variant_t(fRasterRect.lb.x));
		m_SmtRecordset.PutCollect("mbr_ymin",_variant_t(fRasterRect.lb.y));
		m_SmtRecordset.PutCollect("mbr_xmax",_variant_t(fRasterRect.rt.x));
		m_SmtRecordset.PutCollect("mbr_ymax",_variant_t(fRasterRect.rt.y));

		m_SmtRecordset.PutCollect("image_code",_variant_t(lImageCode));
		m_SmtRecordset.PutCollect("image_size",_variant_t(lRasterBufSize));

		char   *pBuf = NULL;
		VARIANT		varBLOB;
		SAFEARRAY	*pSa = NULL;
		SAFEARRAYBOUND rgSaBound[1];
		if(pRasterBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = lRasterBufSize;

			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			if(SafeArrayAccessData(pSa,(void **)&pBuf) == NOERROR)
			{
				memcpy(pBuf,pRasterBuf,lRasterBufSize);
			}
			SafeArrayUnaccessData(pSa);

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  

			m_SmtRecordset.GetField("image_bin")->AppendChunk(varBLOB);

			SafeArrayDestroy(pSa);
		} 

		if (!m_SmtRecordset.Update()) 
			return SMT_ERR_DB_OPER;

		return SMT_ERR_NONE;
	}

	long SmtAdoRasLayer::SyncRead(void)
	{
		long	 lImageCode = -1;
		char	*pRasterBuf = NULL;
		long	 lRasterBufSize = 0;
		fRect	 fRasterRect;

		m_SmtRecordset.GetCollect("mbr_xmin",fRasterRect.lb.x);
		m_SmtRecordset.GetCollect("mbr_ymin",fRasterRect.lb.y);
		m_SmtRecordset.GetCollect("mbr_xmax",fRasterRect.rt.x);
		m_SmtRecordset.GetCollect("mbr_ymax",fRasterRect.rt.y);

		m_SmtRecordset.GetCollect("image_code",lImageCode);
		m_SmtRecordset.GetCollect("image_size",lRasterBufSize);

		long lDataSize = m_SmtRecordset.GetField("image_bin")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("image_bin")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				SafeArrayAccessData(varBLOB.parray,(void **)&pRasterBuf);
				
				long lRtn = m_pMemLayer->CreaterRaster(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);

				SafeArrayUnaccessData (varBLOB.parray);	

				CalEnvelope();
			}
		}

		return SMT_ERR_NONE;
	}
	//////////////////////////////////////////////////////////////////////////

	void  SmtAdoRasLayer::CalEnvelope(void)
	{
		Envelope env;
		m_pMemLayer->CalEnvelope();
		m_pMemLayer->GetEnvelope(env);
		memcpy(&m_lyrEnv,&env,sizeof(Envelope));
	}

	void  SmtAdoRasLayer::SetLayerRect(const fRect &lyrRect)
	{
		m_pMemLayer->SetLayerRect(lyrRect);
		memcpy(&m_lyrEnv,&lyrRect,sizeof(fRect));
	}

	//////////////////////////////////////////////////////////////////////////
	//raster oper
	long SmtAdoRasLayer::SetRasterRect(const fRect &fLocRect)
	{
		SetLayerRect(fLocRect);

		return m_pMemLayer->SetRasterRect(fLocRect);
	}

	long SmtAdoRasLayer::CreaterRaster(const char *pRasterBuf,long lRasterBufSize,const fRect &fRasterRect,long lImageCode)
	{
		if (SMT_ERR_NONE == m_pMemLayer->CreaterRaster(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode))
		{
			CalEnvelope();

			SyncWrite();

			return SMT_ERR_NONE;
		}
	
		return SMT_ERR_FAILURE;
	}

	long SmtAdoRasLayer::GetRaster(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		return m_pMemLayer->GetRaster(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);
	}

	long SmtAdoRasLayer::GetRasterNoClone(char *&pRasterBuf,long &lRasterBufSize,fRect &fRasterRect,long &lImageCode) const
	{
		return m_pMemLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,fRasterRect,lImageCode);
	}

	long SmtAdoRasLayer::GetRasterRect(fRect &fLocRect) const
	{
		return m_pMemLayer->GetRasterRect(fLocRect);
	}
}