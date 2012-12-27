#include <math.h>

#include "gis_api.h"
#include "geo_geometry.h"
#include "smt_logmanager.h"

long CopyLayer(SmtLayer* pTarLayer,SmtLayer* pSrcLayer,bool bClone,bool bCheckFeaType)
{
	if (NULL == pTarLayer || NULL ==pSrcLayer ||
		pSrcLayer->GetLayerType() != pTarLayer->GetLayerType())
		return SMT_ERR_INVALID_PARAM;

	if(pSrcLayer->GetLayerType() == LYR_VECTOR)
		return CopyLayer((SmtVectorLayer*)pTarLayer,(SmtVectorLayer*)pSrcLayer,bClone,bCheckFeaType);
	else if(pSrcLayer->GetLayerType() == LYR_RASTER)
		return CopyLayer((SmtRasterLayer*)pTarLayer,(SmtRasterLayer*)pSrcLayer,bClone,bCheckFeaType);

	return SMT_ERR_FAILURE;
}

long CopyLayer(SmtVectorLayer* pTarLayer,SmtVectorLayer* pSrcLayer,bool bClone,bool bCheckFeaType)
{
	if (!pTarLayer || !pSrcLayer)
		return SMT_ERR_INVALID_PARAM;

	LONGLONG  time1 = 0,time2 = 0,per_cnt;
	QueryPerformanceCounter((LARGE_INTEGER *) &time1);
	QueryPerformanceFrequency((LARGE_INTEGER *) &per_cnt);
	float time = 0.,fTime_scale = 0.;
	fTime_scale = 1./per_cnt;

	pTarLayer->SetLayerFeatureType(pSrcLayer->GetLayerFeatureType());

	pSrcLayer->MoveFirst();
	while(!pSrcLayer->IsEnd())
	{
		SmtFeature *pFeature = pSrcLayer->GetFeature();
		if (NULL != pFeature)
		{
			if (bCheckFeaType)
			{
				if (pFeature->GetFeatureType() == pTarLayer->GetLayerFeatureType())
				{
					int  nID = pTarLayer->GetFeatureCount()+1;	
					pFeature->SetID(nID);
					pTarLayer->AppendFeatureBatch(pFeature,bClone);
				}
			}
			else
			{
				int  nID = pTarLayer->GetFeatureCount()+1;	
				pFeature->SetID(nID);
				pTarLayer->AppendFeature(pFeature,bClone);
				//pTarLayer->AppendFeatureBatch(pFeature,bClone);
			}
		}

		pSrcLayer->MoveNext();
	}
	//pTarLayer->UpdateFeatureBatch();

	pTarLayer->CalEnvelope();

	QueryPerformanceCounter((LARGE_INTEGER *) &time2);
	time = (time2 - time1)*fTime_scale;


	SmtLog *pLog = SmtLogManager::GetSingletonPtr()->GetDefaultLog();
	if (pLog)
	{
		pLog->LogMessage("time cost:%f",time);
	}
	return SMT_ERR_NONE;
}

long CopyLayer(SmtRasterLayer* pTarLayer,SmtRasterLayer* pSrcLayer,bool bClone,bool bCheckFeaType)
{
	if (!pTarLayer || !pSrcLayer)
		return SMT_ERR_INVALID_PARAM;

	LONGLONG  time1 = 0,time2 = 0,per_cnt;
	QueryPerformanceCounter((LARGE_INTEGER *) &time1);
	QueryPerformanceFrequency((LARGE_INTEGER *) &per_cnt);
	float time = 0.,fTime_scale = 0.;
	fTime_scale = 1./per_cnt;

	char		*pRasterBuf = NULL;
	long		lRasterBufSize = 0;
	long		lCodeType = -1;
	fRect		locRect;

	if (SMT_ERR_NONE == pSrcLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,locRect,lCodeType) &&
		SMT_ERR_NONE == pTarLayer->CreaterRaster(pRasterBuf,lRasterBufSize,locRect,lCodeType))
	{
		pTarLayer->CalEnvelope();
	}

	QueryPerformanceCounter((LARGE_INTEGER *) &time2);
	time = (time2 - time1)*fTime_scale;

	SmtLog *pLog = SmtLogManager::GetSingletonPtr()->GetDefaultLog();
	if (pLog)
	{
		pLog->LogMessage("time cost:%f",time);
	}

	return SMT_ERR_NONE;
}
//////////////////////////////////////////////////////////////////////////

long Points2MultiPoint(SmtVectorLayer* pLayer,string strStyle)
{
	if (pLayer->GetLayerFeatureType() != SmtFtDot ||
		pLayer->GetFeatureCount() < 1)
		return SMT_ERR_INVALID_PARAM;
	
	SmtMultiPoint *pMultPoint = new SmtMultiPoint();
	
	pLayer->MoveFirst();
	while(!pLayer->IsEnd())
	{
		SmtFeature *pFeature = pLayer->GetFeature();
		pMultPoint->AddGeometry(pFeature->GetGeometryRef());

		pLayer->MoveNext();
	}

	pLayer->DeleteAll();

	//////////////////////////////////////////////////////////////////////////
	SmtFeature *pSmtFeature = new SmtFeature;

	pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtDot);
	pSmtFeature->SetStyle(strStyle.c_str());
	pSmtFeature->SetGeometryDirectly(pMultPoint);

	if (SMT_ERR_NONE != pLayer->AppendFeature(pSmtFeature,false))
	{
		SMT_SAFE_DELETE(pSmtFeature)
		return SMT_ERR_FAILURE;
	}

	return SMT_ERR_NONE;
}

long GetQueryRs(int geomType,int feaType)
{
	long lQRs = SS_Unkown; 

	if (geomType == GTPoint)
	{
		lQRs = SS_Overlaps|SS_Within; 

		switch (feaType)
		{
		case SmtFtChildImage:
		case SmtFtDot:
		case SmtFtAnno:
		case SmtFtCurve:
			lQRs = SS_Overlaps;
			break;
		case SmtFtSurface:
			lQRs = SS_Within;
			break;
		default:
			break;
		}
	}
	else if (geomType == GTLinearRing)
	{
		lQRs = SS_Contains|SS_Overlaps|SS_Intersects; 

		switch (feaType)
		{
		case SmtFtChildImage:
		case SmtFtDot:
		case SmtFtAnno:
			lQRs = SS_Contains|SS_Overlaps;
			break;

		case SmtFtCurve:
		case SmtFtSurface:
			lQRs = SS_Contains|SS_Intersects;
			break;
		default:
			break;
		}
	}

	return lQRs;
}

