#include "sde_surfacefcls_adoveclayer.h"
#include "smt_api.h"
#include "smt_logmanager.h"
#include "geo_api.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtSurfaceFclsAdoLayer::SmtSurfaceFclsAdoLayer(SmtDataSource *pOwnerDs):SmtAdoVecLayer(pOwnerDs)
	{
		
	}

	SmtSurfaceFclsAdoLayer::~SmtSurfaceFclsAdoLayer(void)
	{

	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtSurfaceFclsAdoLayer::Create(void)
	{
       bool bRet = true;
	   
	   SetDefaultAttFields();
	   SetDefaultGeomFields();
	   SetTableFields();

	   //////////////////////////////////////////////////////////////////////////
	   sprintf(m_szLayerTableName,"%s_SurfaceFcls",m_szLayerName);
	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,m_szLayerTableName);
	   sprintf(info.szName,m_szLayerName);
	   info.unFeatureType = SmtFtSurface;

	   return ((SmtAdoDataSource *)m_pOwnerDs)->CreateLayerTable(info,m_pTableFieldCollect);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtSurfaceFclsAdoLayer::SetDefaultAttFields(void)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);

		smtFld.SetName("area");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pAtt->AddField(smtFld);
	}

	void SmtSurfaceFclsAdoLayer::SetDefaultGeomFields(void)
	{
		SMT_SAFE_DELETE(m_pGeomFieldCollect);
		m_pGeomFieldCollect = new SmtFieldCollect();

		SmtField smtFld;
		//////////////////////////////////////////////////////////////////////////
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

		smtFld.SetName("geom_polygonrings");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_pointnums");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);
	}

	void SmtSurfaceFclsAdoLayer::SetTableFields(void)
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

		smtFld.SetName("geom_polygonrings");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_pointnums");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
		//////////////////////////////////////////////////////////////////////////
		smtFld.SetName("style");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
		//////////////////////////////////////////////////////////////////////////
		smtFld.SetName("area");
		smtFld.SetType(SmtVarType::SmtReal);
		m_pTableFieldCollect->AddField(smtFld);
	}

	void SmtSurfaceFclsAdoLayer::GetFeature(SmtFeature *pSmtFeature)
	{
        if (!pSmtFeature)
			return;

		int       nID = 0;
		double	  dbfArea = 0.;
		SmtStyle *pStyle = NULL;	

		int nGridID = 0;
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

		m_SmtRecordset.GetCollect("area",dbfArea);

		//////////////////////////////////////////////////////////////////////////
		pSmtFeature->SetID(nID);
		pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtSurface);
		pSmtFeature->SetStyleDirectly(pStyle);
		pSmtFeature->SetGeometryDirectly(pGeom);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("area"),dbfArea);
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSurfaceFclsAdoLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bclone)
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
		m_SmtRecordset.PutCollect("area",_variant_t(((SmtSurface*)pSmtFeature->GetGeometryRef())->GetArea()));
		
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

	//////////////////////////////////////////////////////////////////////////
	long SmtSurfaceFclsAdoLayer::AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bclone)
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
		m_SmtRecordset.PutCollect("area",_variant_t(((SmtSurface*)pSmtFeature->GetGeometryRef())->GetArea()));

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);

		return lRet;
	}


	long  SmtSurfaceFclsAdoLayer::UpdateFeature(const SmtFeature *pSmtFeature)
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
					m_SmtRecordset.PutCollect("area",_variant_t(((SmtSurface*)pSmtFeature->GetGeometryRef())->GetArea()));

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
	long   SmtSurfaceFclsAdoLayer::AppendStyle(const SmtStyle *pStyle)
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

    long   SmtSurfaceFclsAdoLayer::AppendGeom(const SmtGeometry *pGeom)
	{
		long lRet = SMT_ERR_NONE;
        SmtGeometryType type  = pGeom->GetGeometryType();
		
		switch(type)
		{
		case GTPolygon:
			lRet = AppendPolygon((SmtPolygon*)pGeom);
			break;

		case GTFan:
			lRet = AppendFan((SmtFan*)pGeom);
			break;

		default:
			break;
		}

		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtSurfaceFclsAdoLayer::GetStyle(SmtStyle *&pStyle)
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

	long   SmtSurfaceFclsAdoLayer::GetGeom(SmtGeometry *&pGeom)
	{
		long lRet = SMT_ERR_NONE;
		int nGeomType = 0;
		m_SmtRecordset.GetCollect("geom_type",nGeomType); 

		switch(nGeomType)
		{

		case GTPolygon:
			lRet = GetPolygon(pGeom);
			break;

		case GTFan:
			lRet = GetFan(pGeom);
			break;

		default:
			break;
		}
		
		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtSurfaceFclsAdoLayer::AppendPolygon(const SmtPolygon *pPolygon)
	{
		 int nRings = pPolygon->GetNumInteriorRings()+1;

		 //////////////////////////////////////////////////////////////////////////
		 m_SmtRecordset.PutCollect("geom_type",_variant_t(GTPolygon));
		 m_SmtRecordset.PutCollect("geom_polygonrings",_variant_t(nRings));

		 //////////////////////////////////////////////////////////////////////////
		 PutCollectLinearingNums(pPolygon);
		 PutCollectLinearings(pPolygon);

		 return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtSurfaceFclsAdoLayer::PutCollectLinearingNums(const SmtPolygon *pPolygon)
	{
		int nInteriorRings = pPolygon->GetNumInteriorRings();

		int *pRingPointNums = new int [nInteriorRings+1];

		pRingPointNums[0] = pPolygon->GetExteriorRing()->GetNumPoints(); 
		for(int i = 0; i < nInteriorRings ; i ++)
			pRingPointNums[i+1] = pPolygon->GetInteriorRing(i)->GetNumPoints();

		char   *pBuf = (char*)pRingPointNums;
		int    nLen  = sizeof(int)*(nInteriorRings+1);
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
			m_SmtRecordset.GetField("geom_pointnums")->AppendChunk(varBLOB);
		} 

		SMT_SAFE_DELETE_A(pRingPointNums);

		return SMT_ERR_NONE;
	}

	long   SmtSurfaceFclsAdoLayer::PutCollectLinearings(const SmtPolygon *pPolygon)
	{
		int nInteriorRings = pPolygon->GetNumInteriorRings();
		int nPoints  = pPolygon->GetExteriorRing()->GetNumPoints();

		for(int i = 0; i < nInteriorRings ; i ++)
			nPoints += pPolygon->GetInteriorRing(i)->GetNumPoints();

		int nLen = sizeof(RawPoint)*nPoints;

		VARIANT varBLOB;
		SAFEARRAY *pSa = NULL;
		SAFEARRAYBOUND rgSaBound[1];

		rgSaBound[0].lLbound = 0;
		rgSaBound[0].cElements = nLen;
		pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound); 

		long nSaEleIndex = 0;
		PutCollectLinearing(pPolygon->GetExteriorRing(),pSa,nSaEleIndex);

		for (int i = 0 ; i < nInteriorRings ; i++)
		  PutCollectLinearing(pPolygon->GetInteriorRing(i),pSa,nSaEleIndex);

		varBLOB.vt = VT_ARRAY | VT_UI1;  
		varBLOB.parray = pSa;  
		m_SmtRecordset.GetField("geom_points")->AppendChunk(varBLOB);

		return SMT_ERR_NONE;
	}

	long   SmtSurfaceFclsAdoLayer::PutCollectLinearing(const SmtLinearRing *pLinearRing,SAFEARRAY *pSa,long &index)
	{
		int nPoints = pLinearRing->GetNumPoints();
		if (nPoints < 0)
			return SMT_ERR_DB_OPER;

		RawPoint *pRawPoints = new RawPoint[nPoints];
		pLinearRing->GetPoints(pRawPoints);

		char   *pBuf = (char*)pRawPoints;
		long    nEndIndex = index + sizeof(RawPoint)*nPoints;

		if(pBuf)
		{
			for (; index < nEndIndex; index++)
				SafeArrayPutElement (pSa, &index, pBuf++);
		} 

		SMT_SAFE_DELETE_A(pRawPoints);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtSurfaceFclsAdoLayer::AppendFan(const SmtFan *pFan)
	{
		//////////////////////////////////////////////////////////////////////////
		m_SmtRecordset.PutCollect("geom_type",_variant_t(GTFan));
		m_SmtRecordset.PutCollect("geom_polygonrings",_variant_t(1));

		//////////////////////////////////////////////////////////////////////////
		int    nPoints = 3;
		char   *pBuf = (char*)(&nPoints);
		int    nLen  = sizeof(int);
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
			m_SmtRecordset.GetField("geom_pointnums")->AppendChunk(varBLOB);
		} 
		SafeArrayDestroy(pSa);

		//////////////////////////////////////////////////////////////////////////
		const SmtArc * pArc =pFan->GetArc();
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

		pBuf = (char*)pRawPoints;
		nLen  = sizeof(RawPoint)*3;

		if(pBuf)
		{ 
			rgSaBound[0].lLbound = 0;
			rgSaBound[0].cElements = nLen;
			pSa = SafeArrayCreate(VT_UI1, 1, rgSaBound);  
			for (long i = 0; i < (long)nLen; i++)
				SafeArrayPutElement (pSa, &i, pBuf++); 

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("geom_points")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);
		SMT_SAFE_DELETE_A(pRawPoints);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtSurfaceFclsAdoLayer::GetPolygon(SmtGeometry *&pGeom)
	{
		  int nRings = 0,nInteriorRings = 0;
		  m_SmtRecordset.GetCollect("geom_polygonrings",nRings);
		  nInteriorRings = nRings - 1;

		  SmtPolygon *pPolygon = new SmtPolygon();
		  pPolygon->AddRingDirectly(new SmtLinearRing());

		  for (int i = 0; i < nInteriorRings ; i ++)
			  pPolygon->AddRingDirectly(new SmtLinearRing());

		  GetCollectLinearingNums(pPolygon);
		  GetCollectLinearings(pPolygon);
		  
		  pGeom = pPolygon;

		  return SMT_ERR_NONE;
	}

	long   SmtSurfaceFclsAdoLayer::GetCollectLinearingNums(SmtPolygon *pPloygon)
	{
		int nInteriorRings = pPloygon->GetNumInteriorRings();
		long lDataSize = m_SmtRecordset.GetField("geom_pointnums")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_pointnums")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				char *pBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
				int nPoints = 0;
				memcpy(&nPoints,pBuf,sizeof(int));
				pBuf += sizeof(int);
				pPloygon->GetExteriorRing()->SetNumPoints(nPoints);

				for (int index = 0; index < nInteriorRings ; index ++)	
				{
					memcpy(&nPoints,pBuf,sizeof(int));
					pPloygon->GetInteriorRing(index)->SetNumPoints(nPoints);
					pBuf += sizeof(int);
				}

				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		return SMT_ERR_NONE;
	}

	long  SmtSurfaceFclsAdoLayer::GetCollectLinearings(SmtPolygon *pPloygon)
	{
		int nInteriorRings = pPloygon->GetNumInteriorRings();
		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				char *pBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
				RawPoint *pRawPointBuf = (RawPoint*)pBuf;

				GetCollectLinearing(pPloygon->GetExteriorRing(),pRawPointBuf);
				for (int  i = 0; i < nInteriorRings ; i++)
				  GetCollectLinearing(pPloygon->GetInteriorRing(i),pRawPointBuf);

				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}
        return SMT_ERR_NONE;
	}

	long   SmtSurfaceFclsAdoLayer::GetCollectLinearing(SmtLinearRing *pLinearRing,RawPoint *&pRawPointBuf)
	{
		int nPoints = pLinearRing->GetNumPoints();
		pLinearRing->SetPoints(nPoints,pRawPointBuf);
		pRawPointBuf += nPoints;

		return SMT_ERR_NONE;
	}


	long   SmtSurfaceFclsAdoLayer::GetFan(SmtGeometry *&pGeom)
	{
		long lDataSize = m_SmtRecordset.GetField("geom_polygonrings")->ActualSize;
		if (lDataSize != sizeof(int))
			return SMT_ERR_DB_OPER;

		//get point nums 
		int nPoints = 0;
		lDataSize = m_SmtRecordset.GetField("geom_pointnums")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_pointnums")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				char *pBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
				memcpy(&nPoints,pBuf,sizeof(int));
				pBuf += sizeof(int);
				SafeArrayUnaccessData (varBLOB.parray);	
			}
		}

		if (nPoints != 3)
			return SMT_ERR_DB_OPER;


		SmtArc *pArc = new SmtArc();
		RawPoint *pRawPoints = new RawPoint[3];

		lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
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

		SmtFan * pFan = new SmtFan();
		pFan->SetArcDirectly(pArc);
		pGeom = pFan;

		return SMT_ERR_NONE;
	}
}