#include "sde_dotfcls_adoveclayer.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtDotFclsAdoLayer::SmtDotFclsAdoLayer(SmtDataSource *pOwnerDs):SmtAdoVecLayer(pOwnerDs)
	{
		;
	}

	SmtDotFclsAdoLayer::~SmtDotFclsAdoLayer(void)
	{

	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtDotFclsAdoLayer::Create(void)
	{
       bool bRet = true;

	   SetDefaultAttFields();
	   SetDefaultGeomFields();
	   SetTableFields();

	   //////////////////////////////////////////////////////////////////////////
	   sprintf(m_szLayerTableName,"%s_DotFcls",m_szLayerName);
	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,m_szLayerTableName);
	   sprintf(info.szName,m_szLayerName);
	   info.unFeatureType = SmtFtDot;

	   return ((SmtAdoDataSource *)m_pOwnerDs)->CreateLayerTable(info,m_pTableFieldCollect);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtDotFclsAdoLayer::SetDefaultAttFields(void)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);
	}

	void SmtDotFclsAdoLayer::SetDefaultGeomFields(void)
	{
		SMT_SAFE_DELETE(m_pGeomFieldCollect);
		m_pGeomFieldCollect = new SmtFieldCollect();

		SmtField smtFld;

		//geom
		smtFld.SetName("geom_type");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

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

		smtFld.SetName("grid_id");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_pointnum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);
	}

	void SmtDotFclsAdoLayer::SetTableFields(void)
	{
		SMT_SAFE_DELETE(m_pTableFieldCollect);
		m_pTableFieldCollect = new SmtFieldCollect();

		SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		//////////////////////////////////////////////////////////////////////////
		//geom
		smtFld.SetName("geom_type");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

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

		smtFld.SetName("grid_id");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_pointnum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
		//////////////////////////////////////////////////////////////////////////

		smtFld.SetName("style");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtDotFclsAdoLayer::GetFeature(SmtFeature *pSmtFeature)
	{
		if (!pSmtFeature)
			return;

		int nID = 0;
		int nGridID = 0;
		SmtStyle *pStyle = NULL;

		Envelope env;
		m_SmtRecordset.GetCollect("ID",nID); 
		m_SmtRecordset.GetCollect("mbr_xmin",env.MinX);
		m_SmtRecordset.GetCollect("mbr_ymin",env.MinY);
		m_SmtRecordset.GetCollect("mbr_xmax",env.MaxX);
		m_SmtRecordset.GetCollect("mbr_ymax",env.MaxY);
		m_SmtRecordset.GetCollect("grid_id",nGridID);

		GetStyle(pStyle); 
		//////////////////////////////////////////////////////////////////////////
		//geom
		SmtGeometry *pGeom = NULL;
		GetGeom(pGeom);
		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		pSmtFeature->SetID(nID);
		pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtDot);
		pSmtFeature->SetStyleDirectly(pStyle);
		pSmtFeature->SetGeometryDirectly(pGeom);
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtDotFclsAdoLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bclone)
	{
		long lRet = SMT_ERR_NONE;
		//////////////////////////////////////////////////////////////////////////
		//db
		if (!IsOpen())
			if (!Open(m_szLayerTableName))
				return SMT_ERR_DB_OPER;

		int nGridID = 0;
		Envelope env;
		const SmtGeometry *pGeom = pSmtFeature->GetGeometryRef();
		const SmtAttribute *pAtt = pSmtFeature->GetAttributeRef();
		const SmtStyle	*pStyle	 = pSmtFeature->GetStyle();

		pGeom->GetEnvelope(&env);

		m_SmtRecordset.AddNew();	
		m_SmtRecordset.PutCollect("ID",_variant_t(pSmtFeature->GetID()));
		m_SmtRecordset.PutCollect("mbr_xmin",_variant_t(env.MinX));
		m_SmtRecordset.PutCollect("mbr_ymin",_variant_t(env.MinY));
		m_SmtRecordset.PutCollect("mbr_xmax",_variant_t(env.MaxX));
		m_SmtRecordset.PutCollect("mbr_ymax",_variant_t(env.MaxY));
		m_SmtRecordset.PutCollect("grid_id",_variant_t(nGridID));

		//////////////////////////////////////////////////////////////////////////
		lRet = AppendStyle(pStyle);

		//////////////////////////////////////////////////////////////////////////
		lRet = AppendGeom(pGeom);

		//////////////////////////////////////////////////////////////////////////
		if (!m_SmtRecordset.Update()) 
		{
			SMT_SAFE_DELETE(pSmtFeature);
			return SMT_ERR_DB_OPER;
		}

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);
		CalEnvelope();

		return lRet;
	}

	long SmtDotFclsAdoLayer::AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bclone)
	{
		long lRet = SMT_ERR_NONE;
		//////////////////////////////////////////////////////////////////////////
		//db
		if (!IsOpen())
			if (!Open(m_szLayerTableName))
				return SMT_ERR_DB_OPER;

		SmtPoint *oPoint = (SmtPoint *)(pSmtFeature->GetGeometryRef());
		int nGridID = 0;
		Envelope env;

		const SmtGeometry *pGeom = pSmtFeature->GetGeometryRef();
		const SmtAttribute *pAtt = pSmtFeature->GetAttributeRef();
		const SmtStyle	*pStyle = pSmtFeature->GetStyle();

		m_SmtRecordset.AddNew();	
		m_SmtRecordset.PutCollect("ID",_variant_t(pSmtFeature->GetID()));
		m_SmtRecordset.PutCollect("mbr_xmin",_variant_t(env.MinX));
		m_SmtRecordset.PutCollect("mbr_ymin",_variant_t(env.MinY));
		m_SmtRecordset.PutCollect("mbr_xmax",_variant_t(env.MaxX));
		m_SmtRecordset.PutCollect("mbr_ymax",_variant_t(env.MaxY));
		m_SmtRecordset.PutCollect("grid_id",_variant_t(nGridID));
		//////////////////////////////////////////////////////////////////////////
		lRet = AppendStyle(pStyle);

		//////////////////////////////////////////////////////////////////////////
		lRet = AppendGeom(pGeom);

		//////////////////////////////////////////////////////////////////////////

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);
		CalEnvelope();

		return lRet;
	}

	long  SmtDotFclsAdoLayer::UpdateFeature(const SmtFeature *pSmtFeature)
	{
		//////////////////////////////////////////////////////////////////////////
		//db
		SmtFeature *pSmtFea = m_pMemLayer->GetFeature();
		if (NULL != pSmtFea)
		{
			char szSQL[TEMP_BUFFER_SIZE];
			sprintf_s(szSQL,TEMP_BUFFER_SIZE,"select * from %s where ID = %d;",m_szLayerTableName,pSmtFea->GetID());
			if (m_SmtRecordset.Open(szSQL))
			{
				m_SmtRecordset.MoveFirst();
				if (m_SmtRecordset.IsOpen() && !m_SmtRecordset.IsEOF() && m_SmtRecordset.GetRecordCount() == 1)
				{
					int nGridID = 0;
					Envelope env;
					const SmtGeometry *pGeom = pSmtFeature->GetGeometryRef();
					const SmtAttribute *pAtt = pSmtFeature->GetAttributeRef();
					const SmtStyle	*pStyle	 = pSmtFeature->GetStyle();

					pGeom->GetEnvelope(&env);

					m_SmtRecordset.AddNew();	
					m_SmtRecordset.PutCollect("ID",_variant_t(pSmtFeature->GetID()));
					m_SmtRecordset.PutCollect("mbr_xmin",_variant_t(env.MinX));
					m_SmtRecordset.PutCollect("mbr_ymin",_variant_t(env.MinY));
					m_SmtRecordset.PutCollect("mbr_xmax",_variant_t(env.MaxX));
					m_SmtRecordset.PutCollect("mbr_ymax",_variant_t(env.MaxY));
					m_SmtRecordset.PutCollect("grid_id",_variant_t(nGridID));

					//////////////////////////////////////////////////////////////////////////
					AppendStyle(pStyle);

					//////////////////////////////////////////////////////////////////////////
					AppendGeom(pGeom);

					if (!m_SmtRecordset.Update()) 
						return SMT_ERR_DB_OPER;

					//mem
					m_pMemLayer->UpdateFeature(pSmtFeature);
					CalEnvelope();
				}
			}
		}

		return SMT_ERR_NONE;
	}

	long   SmtDotFclsAdoLayer::AppendStyle(const SmtStyle *pStyle)
	{
		char   *pBuf = (char*)pStyle;
		int    nLen  = sizeof(SmtStyle);

		VARIANT varBLOB;
		SAFEARRAY *pSa = NULL;
		SAFEARRAYBOUND rgSaBound[1];
		if(pBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = nLen;

			char *pSABuf = NULL;
			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			if(SafeArrayAccessData(pSa,(void **)&pSABuf) == NOERROR)
			{
				memcpy(pSABuf,pBuf,nLen);
			}
			SafeArrayUnaccessData(pSa);

			/*pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			for (long i = 0; i < (long)nLen; i++)
				SafeArrayPutElement (pSa, &i, pBuf++); */

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("style")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);
		return SMT_ERR_NONE;
	}

	long   SmtDotFclsAdoLayer::GetStyle(SmtStyle *&pStyle)
	{
		pStyle = new SmtStyle();
		long lDataSize = m_SmtRecordset.GetField("style")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("style")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				SmtStyle *pBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
				int nSize = sizeof(SmtStyle);
				memcpy(pStyle,pBuf,sizeof(SmtStyle));
				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtDotFclsAdoLayer::AppendGeom(const SmtGeometry *pGeom)
	{
		long lRet = SMT_ERR_NONE;
		SmtGeometryType type  = pGeom->GetGeometryType();

		switch(type)
		{
		case GTPoint:
			lRet = AppendPoint((SmtPoint*)pGeom);
			break;
		case GTMultiPoint:
			lRet = AppendPoints((SmtMultiPoint*)pGeom);
		default:
			break;
		}

		return lRet;
	}

	long   SmtDotFclsAdoLayer::GetGeom(SmtGeometry *&pGeom)
	{
		long lRet = SMT_ERR_NONE;
		int nGeomType = 0;
		m_SmtRecordset.GetCollect("geom_type",nGeomType); 

		switch(nGeomType)
		{
		case GTPoint:
			lRet = GetPoint(pGeom);
			break;
		case GTMultiPoint:
			lRet = GetPoints(pGeom);
		default:
			break;
		}

		return lRet;
	}

	long SmtDotFclsAdoLayer::AppendPoint(const SmtPoint *pPoint)
	{
		int   nPoints = 1;

		if (nPoints < 0)
			return SMT_ERR_DB_OPER;

		RawPoint *pRawPoints = new RawPoint[nPoints];
		pRawPoints[0].x = pPoint->GetX();
		pRawPoints[0].y = pPoint->GetY();

		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTPoint));
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(nPoints));

		//////////////////////////////////////////////////////////////////////////

		char   *pBuf = (char*)pRawPoints;
		int    nLen  = sizeof(RawPoint)*nPoints;

		VARIANT varBLOB;
		SAFEARRAY *pSa = NULL;
		SAFEARRAYBOUND rgSaBound[1];
		if(pBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = nLen;

			char *pSABuf = NULL;
			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			if(SafeArrayAccessData(pSa,(void **)&pSABuf) == NOERROR)
			{
				memcpy(pSABuf,pBuf,nLen);
			}
			SafeArrayUnaccessData(pSa);

			/*pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			for (long i = 0; i < (long)nLen; i++)
				SafeArrayPutElement (pSa, &i, pBuf++); */

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("geom_points")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);

		SMT_SAFE_DELETE_A(pRawPoints);

		return SMT_ERR_NONE;
	}

	long SmtDotFclsAdoLayer::AppendPoints(const SmtMultiPoint *pPoints)
	{
		int   nPoints = pPoints->GetNumGeometries();

		if (nPoints < 0)
			return SMT_ERR_DB_OPER;

		RawPoint *pRawPoints = new RawPoint[nPoints];

		for (int i = 0; i < nPoints;i++)
		{
			const SmtPoint *pPoint = (SmtPoint *)pPoints->GetGeometryRef(i);
			pRawPoints[i].x = pPoint->GetX();
			pRawPoints[i].y = pPoint->GetY();
		}
		
		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTMultiPoint));
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(nPoints));

		//////////////////////////////////////////////////////////////////////////

		char   *pBuf = (char*)pRawPoints;
		int    nLen  = sizeof(RawPoint)*nPoints;

		VARIANT varBLOB;
		SAFEARRAY *pSa = NULL;
		SAFEARRAYBOUND rgSaBound[1];
		if(pBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = nLen;

			char *pSABuf = NULL;
			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			if(SafeArrayAccessData(pSa,(void **)&pSABuf) == NOERROR)
			{
				memcpy(pSABuf,pBuf,nLen);
			}
			SafeArrayUnaccessData(pSa);

		/*	pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			for (long i = 0; i < (long)nLen; i++)
				SafeArrayPutElement (pSa, &i, pBuf++); */

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("geom_points")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);
		SMT_SAFE_DELETE_A(pRawPoints);
		return SMT_ERR_NONE;
	}

	long SmtDotFclsAdoLayer::GetPoint(SmtGeometry *&pGeom)
	{
		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);
		if (nPoints != 1)
			return SMT_ERR_DB_OPER;

		SmtPoint *pPoint = new SmtPoint();

		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				RawPoint *pRawPointBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);

				pPoint->SetX(pRawPointBuf->x);
				pPoint->SetY(pRawPointBuf->y);

				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		pGeom = pPoint;

		return SMT_ERR_NONE;
	}	

	long SmtDotFclsAdoLayer::GetPoints(SmtGeometry *&pGeom)
	{
		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);
	
		if (nPoints < 1)
			return SMT_ERR_DB_OPER;
		 
		SmtMultiPoint *pPoints = new SmtMultiPoint();

		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				RawPoint *pRawPointBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);

				for (int index = 0; index < nPoints ; index ++)	
				{
					SmtPoint *pPoint = new SmtPoint();
					pPoint->SetX(pRawPointBuf->x);
					pPoint->SetY(pRawPointBuf->y);
					pPoints->AddGeometryDirectly(pPoint);

					pRawPointBuf ++;
				}

				SafeArrayUnaccessData (varBLOB.parray);
			}
		}

		pGeom = pPoints;

		return SMT_ERR_NONE;
	}	
}