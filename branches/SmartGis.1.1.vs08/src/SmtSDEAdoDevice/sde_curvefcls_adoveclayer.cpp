#include "sde_curvefcls_adoveclayer.h"
#include "smt_api.h"
#include "geo_api.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtCurveFclsAdoLayer::SmtCurveFclsAdoLayer(SmtDataSource *pOwnerDs):SmtAdoVecLayer(pOwnerDs)
	{
		;
	}

	SmtCurveFclsAdoLayer::~SmtCurveFclsAdoLayer(void)
	{

	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtCurveFclsAdoLayer::Create(void)
	{
       bool bRet = true;
	  
	   SetDefaultAttFields();
	   SetDefaultGeomFields();
	   SetTableFields();

	   sprintf(m_szLayerTableName,"%s_CurveFcls",m_szLayerName);
	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,m_szLayerTableName);
	   sprintf(info.szName,m_szLayerName);
	   info.unFeatureType = SmtFtCurve;

	   return ((SmtAdoDataSource *)m_pOwnerDs)->CreateLayerTable(info,m_pTableFieldCollect);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtCurveFclsAdoLayer::SetDefaultAttFields(void)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);

		smtFld.SetName("length");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pAtt->AddField(smtFld);
	}

	void SmtCurveFclsAdoLayer::SetDefaultGeomFields(void)
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

		smtFld.SetName("line_anlytype");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_pointnum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);
	}

	void SmtCurveFclsAdoLayer::SetTableFields(void)
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

		smtFld.SetName("line_anlytype");
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
		//////////////////////////////////////////////////////////////////////////
		smtFld.SetName("length");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtCurveFclsAdoLayer::GetFeature(SmtFeature *pSmtFeature)
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

		GetStyle(pStyle) ;

		//////////////////////////////////////////////////////////////////////////
		SmtGeometry *pGeom = NULL;
		GetGeom(pGeom);
		//////////////////////////////////////////////////////////////////////////
		pSmtFeature->SetID(nID);
		pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtCurve);
		pSmtFeature->SetStyleDirectly(pStyle);
		pSmtFeature->SetGeometryDirectly(pGeom);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pGeom)->GetLength());
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtCurveFclsAdoLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bclone)
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
		const SmtStyle	*pStyle = pSmtFeature->GetStyle();

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
		m_SmtRecordset.PutCollect("length",_variant_t(((SmtCurve*)pSmtFeature->GetGeometryRef())->GetLength()));
		
		if (lRet != SMT_ERR_DB_OPER && !m_SmtRecordset.Update()) 
		{
			SMT_SAFE_DELETE(pSmtFeature);
			return SMT_ERR_DB_OPER;
		}

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);
		CalEnvelope();
		 
		return lRet;
	}

	long SmtCurveFclsAdoLayer::AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bclone)
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
		const SmtStyle	*pStyle = pSmtFeature->GetStyle();

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
		m_SmtRecordset.PutCollect("length",_variant_t(((SmtCurve*)pSmtFeature->GetGeometryRef())->GetLength()));

		if (lRet != SMT_ERR_DB_OPER && !m_SmtRecordset.Update()) 
		{
			SMT_SAFE_DELETE(pSmtFeature);
			return SMT_ERR_DB_OPER;
		}

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);
		CalEnvelope();

		return lRet;
	}

	long  SmtCurveFclsAdoLayer::UpdateFeature(const SmtFeature *pSmtFeature)
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
					const SmtStyle	*pStyle = pSmtFeature->GetStyle();

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

					//////////////////////////////////////////////////////////////////////////
					m_SmtRecordset.PutCollect("length",_variant_t(((SmtCurve*)pSmtFeature->GetGeometryRef())->GetLength()));

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

	//////////////////////////////////////////////////////////////////////////
	long   SmtCurveFclsAdoLayer::AppendStyle(const SmtStyle *pStyle)
	{
		char   *pBuf = (char*)pStyle;
		int    nLen  = sizeof(SmtStyle);

		VARIANT varBLOB;
		SAFEARRAY *pSa;
		SAFEARRAYBOUND rgSaBound[1];
		if(pBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = nLen;
			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			for (long i = 0; i < (long)nLen; i++)
				SafeArrayPutElement (pSa, &i, pBuf++); 

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("style")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);
		return SMT_ERR_NONE;
	}

    long   SmtCurveFclsAdoLayer::AppendGeom(const SmtGeometry * pGeom)
	{
		long lRet = SMT_ERR_NONE;
        SmtGeometryType type  = pGeom->GetGeometryType();
		
		switch(type)
		{
		 
		case GTLineString:
			lRet = AppendLineString((SmtLineString*)pGeom);
			break;

		case GTLinearRing:
			lRet = AppendLinearRing((SmtLinearRing*)pGeom);
			break;

		case GTSpline:
			lRet = AppendSpline((SmtSpline*)pGeom);
			break;

		case GTArc:
			lRet = AppendArc((SmtArc*)pGeom);
			break;

		default:
			break;
		}

		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtCurveFclsAdoLayer::GetStyle(SmtStyle * &pStyle)
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
				memcpy(pStyle,pBuf,sizeof(SmtStyle));
				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		return SMT_ERR_NONE;
	}

	long   SmtCurveFclsAdoLayer::GetGeom(SmtGeometry * &pGeom)
	{
		long lRet = SMT_ERR_NONE;
		int nGeomType = 0;
		m_SmtRecordset.GetCollect("geom_type",nGeomType); 

		switch(nGeomType)
		{

		case GTLineString:
			lRet = GetLineString(pGeom);
			break;

		case GTLinearRing:
			lRet = GetLinearRing(pGeom);
			break;

		case GTSpline:
			lRet = GetSpline(pGeom);
			break;

		case GTArc:
			lRet = GetArc(pGeom);
			break;

		default:
			break;
		}
		
		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtCurveFclsAdoLayer::AppendLineString(const SmtLineString * pLineString)
	{
         int   nPoints = pLineString->GetNumPoints();

		 if (nPoints < 0)
			 return SMT_ERR_DB_OPER;
		 
		 RawPoint *pRawPoints = new RawPoint[nPoints];
		 pLineString->GetPoints(pRawPoints);
		 //////////////////////////////////////////////////////////////////////////
		 m_SmtRecordset.PutCollect("geom_type",_variant_t(GTLineString));
		 m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(nPoints));
		 m_SmtRecordset.PutCollect("line_anlytype",_variant_t(-1));

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

			/* pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
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

	long   SmtCurveFclsAdoLayer::AppendSpline(const SmtSpline * pSpline)
	{
		int   nPoints = pSpline->GetNumPoints();

		if (nPoints < 0)
			return SMT_ERR_DB_OPER;

		RawPoint *pRawPoints = new RawPoint[nPoints];
		pSpline->GetPoints(pRawPoints);
		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTSpline));
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(nPoints));
		m_SmtRecordset.PutCollect("line_anlytype",_variant_t(pSpline->GetAnalyticType()));

		//////////////////////////////////////////////////////////////////////////

		char   *pBuf = (char*)pRawPoints;
		int    nLen  = sizeof(RawPoint)*nPoints;

		VARIANT varBLOB;
		SAFEARRAY *pSa;
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

	long   SmtCurveFclsAdoLayer::AppendLinearRing(const SmtLinearRing * pLinearRing)
	{
		int   nPoints = pLinearRing->GetNumPoints();

		if (nPoints < 0)
			return SMT_ERR_DB_OPER;

		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTLinearRing));
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(nPoints));
		m_SmtRecordset.PutCollect("line_anlytype",_variant_t(pLinearRing->GetAnalyticType()));

		//////////////////////////////////////////////////////////////////////////
		RawPoint *pRawPoints = new RawPoint[nPoints];
		pLinearRing->GetPoints(pRawPoints);

		char   *pBuf = (char*)pRawPoints;
		int    nLen  = sizeof(RawPoint)*nPoints;

		VARIANT varBLOB;
		SAFEARRAY *pSa;
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

	long   SmtCurveFclsAdoLayer::AppendArc(const SmtArc * pArc)
	{
		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTArc));
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(3));
		m_SmtRecordset.PutCollect("line_anlytype",_variant_t(-1));

		//////////////////////////////////////////////////////////////////////////

		SmtPoint oPoint[3];
		RawPoint *pRawPoints = new RawPoint[3];
		pArc->GetCenterPoint(&oPoint[0]);
		pArc->StartPoint(&oPoint[1]);
		pArc->EndPoint(&oPoint[2]);

		pRawPoints[0].x = oPoint[0].GetX();
		pRawPoints[0].y = oPoint[0].GetY();

		pRawPoints[1].x = oPoint[1].GetX();
		pRawPoints[1].y = oPoint[1].GetY();

		pRawPoints[2].x = oPoint[2].GetX();
		pRawPoints[2].y = oPoint[2].GetY();

		char   *pBuf = (char*)pRawPoints;
		int    nLen  = sizeof(RawPoint)*3;

		VARIANT varBLOB;
		SAFEARRAY *pSa;
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

			SafeArrayDestroy(pSa);
		} 

		SMT_SAFE_DELETE_A(pRawPoints);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtCurveFclsAdoLayer::GetLineString(SmtGeometry * &pGeom)
	{
          SmtLineString *pLineString = new SmtLineString();

		  int nPoints = 0;
		  m_SmtRecordset.GetCollect("geom_pointnum",nPoints);
		 
		  pLineString->SetNumPoints(nPoints);

		  long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		  if(lDataSize > 0)
		  {
			  _variant_t varBLOB;
			  varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			  if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			  {
				  RawPoint *pRawPointBuf = NULL;
				  SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);
				  pLineString->SetPoints(nPoints,pRawPointBuf);
				  SafeArrayUnaccessData (varBLOB.parray);	
			  }
		  }

		  pGeom = pLineString;

		  return SMT_ERR_NONE;
	}

	long   SmtCurveFclsAdoLayer::GetSpline(SmtGeometry * &pGeom)
	{
		SmtSpline *pSpline = new SmtSpline();

		int nAnlyType = 0;
		m_SmtRecordset.GetCollect("line_anlytype",nAnlyType); 

		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);

		pSpline->SetNumPoints(nPoints);
		pSpline->SetAnalyticType(nAnlyType);

		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				RawPoint *pRawPointBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);
				pSpline->SetPoints(nPoints,pRawPointBuf);
				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		pSpline->CalcAnalyticPoints();

		pGeom = pSpline;

		return SMT_ERR_NONE;
	}


	long   SmtCurveFclsAdoLayer::GetLinearRing(SmtGeometry * &pGeom)
	{
		SmtLinearRing *pLinearRing = new SmtLinearRing();

		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);

		pLinearRing->SetNumPoints(nPoints);

		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				RawPoint *pRawPointBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);
				pLinearRing->SetPoints(nPoints,pRawPointBuf);
				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		pLinearRing->CloseRings();

		pGeom = pLinearRing;

		return SMT_ERR_NONE;
	}

	long   SmtCurveFclsAdoLayer::GetArc(SmtGeometry * &pGeom)
	{
		SmtArc *pArc = new SmtArc();
		RawPoint *pRawPoints = new RawPoint[3];

		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);

		if (nPoints != 3)
			return SMT_ERR_DB_OPER;


		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				char *pBuf = NULL;
				RawPoint *pRawPoints1 = pRawPoints;
				SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
				for (int index = 0; index < nPoints ; index ++)	
				{
					memcpy(pRawPoints1,pBuf,sizeof(RawPoint));
					pRawPoints1 ++;
					pBuf +=sizeof(RawPoint);
				}

				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		pArc->SetCenterPoint(pRawPoints[0]);
		pArc->SetStartPoint(pRawPoints[1]);
		pArc->SetEndPoint(pRawPoints[2]);

		float r = SmtDistance(pRawPoints[0].x,pRawPoints[0].y,pRawPoints[1].x,pRawPoints[1].y);
		pArc->SetRadius(r);

		SMT_SAFE_DELETE_A(pRawPoints);
		pGeom = pArc;

		return SMT_ERR_NONE;
	}
}