#include "sde_tinfcls_adoveclayer.h"
#include "smt_api.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

namespace Smt_SDEAdo
{
	SmtTinFclsAdoLayer::SmtTinFclsAdoLayer(SmtDataSource *pOwnerDs):SmtAdoVecLayer(pOwnerDs)
	{
		
	}

	SmtTinFclsAdoLayer::~SmtTinFclsAdoLayer(void)
	{

	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtTinFclsAdoLayer::Create(void)
	{
       bool bRet = true;

	   SetDefaultAttFields();
	   SetDefaultGeomFields();
	   SetTableFields();

	   //////////////////////////////////////////////////////////////////////////
	   sprintf(m_szLayerTableName,"%s_TinFcls",m_szLayerName);
	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,m_szLayerTableName);
	   sprintf(info.szName,m_szLayerName);
	   info.unFeatureType = SmtFtTin;

	   return ((SmtAdoDataSource *)m_pOwnerDs)->CreateLayerTable(info,m_pTableFieldCollect);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtTinFclsAdoLayer::SetDefaultAttFields(void)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;

		smtFld.SetName("ID");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);

		smtFld.SetName("point_num");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);

		smtFld.SetName("triangle_num");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pAtt->AddField(smtFld);
	}

	void SmtTinFclsAdoLayer::SetDefaultGeomFields(void)
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

		smtFld.SetName("geom_trianglenum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pGeomFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_triangles");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pGeomFieldCollect->AddField(smtFld);
	}

	void SmtTinFclsAdoLayer::SetTableFields(void)
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

		smtFld.SetName("geom_pointnum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_points");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_trianglenum");
		smtFld.SetType(SmtVarType::SmtInteger);
		m_pTableFieldCollect->AddField(smtFld);

		smtFld.SetName("geom_triangles");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);

		//////////////////////////////////////////////////////////////////////////

		smtFld.SetName("style");
		smtFld.SetType(SmtVarType::SmtBinary);
		m_pTableFieldCollect->AddField(smtFld);
	}

	void SmtTinFclsAdoLayer::GetFeature(SmtFeature *pSmtFeature)
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
		pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtTin);
		pSmtFeature->SetStyleDirectly(pStyle);
		pSmtFeature->SetGeometryDirectly(pGeom);
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtTinFclsAdoLayer::AppendFeature(const SmtFeature *pSmtFeature,bool bclone)
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

	long SmtTinFclsAdoLayer::AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bclone)
	{
		long lRet = SMT_ERR_NONE;
		//////////////////////////////////////////////////////////////////////////
		//db
		if (!IsOpen() && !Open(m_szLayerTableName))
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

		//mem
		m_pMemLayer->AppendFeature(pSmtFeature,bclone);
		CalEnvelope();

		return lRet;
	}

	long  SmtTinFclsAdoLayer::UpdateFeature(const SmtFeature *pSmtFeature)
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
	long   SmtTinFclsAdoLayer::AppendStyle(const SmtStyle *pStyle)
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

    long   SmtTinFclsAdoLayer::AppendGeom(const SmtGeometry *pGeom)
	{
		long lRet = SMT_ERR_NONE;
        SmtGeometryType type  = pGeom->GetGeometryType();
		
		switch(type)
		{
		case GTTin:
			{
				lRet = AppendTin((SmtTin*)pGeom);
			}
			break;
		default:
			break;
		}


		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtTinFclsAdoLayer::GetStyle(SmtStyle *&pStyle)
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

	long   SmtTinFclsAdoLayer::GetGeom(SmtGeometry *&pGeom)
	{
		long lRet = SMT_ERR_NONE;
		int nMeshType = 0;
		m_SmtRecordset.GetCollect("geom_type",nMeshType); 

		switch(nMeshType)
		{
		case GTTin:
			{
                lRet = GetTin(pGeom);
			}
		default:
			break;
		}
		
		return lRet;
	}

	//////////////////////////////////////////////////////////////////////////
	long   SmtTinFclsAdoLayer::AppendTin(const SmtTin *pTin)
	{
		 m_SmtRecordset.PutCollect("geom_type",_variant_t(GTTin));

		 AppendTinTriangles(pTin);
		
		 AppendTinNodes(pTin);

		 return SMT_ERR_NONE;
	}

	long  SmtTinFclsAdoLayer::AppendTinTriangles(const SmtTin *pTin)
	{
		m_SmtRecordset.PutCollect("geom_trianglenum",_variant_t(pTin->GetTriangleCount()));

		int nTrianges = pTin->GetTriangleCount();

		if (nTrianges < 1)
			return SMT_ERR_DB_OPER;

		SmtTriangle *pTriBuf = new SmtTriangle[nTrianges];

		for (int i = 0; i < pTin->GetTriangleCount();i++)
			pTriBuf[i] = pTin->GetTriangle(i);
		
		char   *pBuf = (char*)pTriBuf;
		int    nLen  = sizeof(SmtTriangle)*nTrianges;

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
				SafeArrayPutElement (pSa, &i, pBuf++);*/ 

			varBLOB.vt = VT_ARRAY | VT_UI1;  
			varBLOB.parray = pSa;  
			m_SmtRecordset.GetField("geom_triangles")->AppendChunk(varBLOB);
		} 

		SafeArrayDestroy(pSa);

		SMT_SAFE_DELETE_A(pTriBuf);

		return SMT_ERR_NONE;
	}

	long  SmtTinFclsAdoLayer::AppendTinNodes(const SmtTin *pTin)
	{
		m_SmtRecordset.PutCollect("geom_pointnum",_variant_t(pTin->GetPointCount()));

		int nPoints = pTin->GetPointCount();
	
		if (nPoints < 1)
			return SMT_ERR_DB_OPER;

		RawPoint *pRawPoints = new RawPoint[nPoints];

		for (int i = 0; i < pTin->GetPointCount();i++)
		{
			SmtPoint oPoint = pTin->GetPoint(i);
			pRawPoints[i].x = oPoint.GetX();
			pRawPoints[i].y = oPoint.GetY();
		}

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

	//////////////////////////////////////////////////////////////////////////
	long   SmtTinFclsAdoLayer::GetTin(SmtGeometry *&pGeom)
	{
		  SmtTin *pTin = new SmtTin();

		  GetTinTriangles(pTin);	
		  GetTinNodes(pTin);
		  
		  pGeom = pTin;

		  return SMT_ERR_NONE;
	}

	long   SmtTinFclsAdoLayer::GetTinTriangles(SmtTin *&pTin)
	{
		int nTriangles = 0;
		m_SmtRecordset.GetCollect("geom_trianglenum",nTriangles);

		long lDataSize = m_SmtRecordset.GetField("geom_triangles")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_triangles")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				SmtTriangle *pTriBuf = NULL;
				SafeArrayAccessData(varBLOB.parray,(void **)&pTriBuf);

				pTin->AddTriangleCollection(pTriBuf,nTriangles);

				SafeArrayUnaccessData (varBLOB.parray);
			}
		}

		return SMT_ERR_NONE;
	}

	long   SmtTinFclsAdoLayer::GetTinNodes(SmtTin *&pTin)
	{
		int nPoints = 0;
		m_SmtRecordset.GetCollect("geom_pointnum",nPoints);

		long lDataSize = m_SmtRecordset.GetField("geom_points")->ActualSize;
		if(lDataSize > 0)
		{
			_variant_t varBLOB;
			varBLOB = m_SmtRecordset.GetField("geom_points")->GetChunk(lDataSize);
			if(varBLOB.vt == (VT_ARRAY | VT_UI1)) 
			{
				RawPoint *pRawPointBuf = NULL;
				SmtPoint *pPointBuf = new SmtPoint[nPoints];
				SafeArrayAccessData(varBLOB.parray,(void **)&pRawPointBuf);
				
				for (int i = 0 ; i < nPoints;i++)
				{
					pPointBuf[i].SetX(pRawPointBuf[i].x);
					pPointBuf[i].SetY(pRawPointBuf[i].y);
				}
			
				SafeArrayUnaccessData (varBLOB.parray);	

				//
				pTin->AddPointCollection(pPointBuf,nPoints);

				SMT_SAFE_DELETE_A(pPointBuf);
			}
		}
		
		return SMT_ERR_NONE;
	}
}