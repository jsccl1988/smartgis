#include "sde_smf_ogrsupport.h"
#include "smt_api.h"
#include "sde_mem.h"
#include <set>
using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_SDEMem;
using namespace Smt_GIS;

bool CopyOGRGeomToSmtGeom(OGRFeature *pOGRFea,SmtFeature  *pSmtFea);
bool CopyOGRAttToSmtAtt(OGRFeature *pOGRFea,SmtFeature  *pSmtFea);

void InitOGRFldTypeToSmtFldType(map<int,int> &ogrFldTypeMap)
{
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTInteger,SmtInteger));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTReal,SmtReal));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTInteger,SmtByte));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTString,SmtString));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTTime,SmtTime));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTDateTime,SmtDateTime));
}

void InitOGRFeaTypeToSmtFeaType(map<int,int> &ogrFeaTypeMap)
{
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbPoint,SmtFtDot));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbLineString,SmtFtCurve));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbPolygon,SmtFtSurface));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbMultiPoint,SmtFtDot));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbMultiLineString,SmtFtCurve));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbMultiPolygon,SmtFtSurface));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbLinearRing,SmtFtCurve));
	ogrFeaTypeMap.insert(map<int,int>::value_type(wkbNone,SmtFtUnknown));
}

void OGRFldTypeToSmtFldType(long ogrType,long &smtType)
{
	static	map<int,int>	ogrFldTypeMap;
	static bool init = (InitOGRFldTypeToSmtFldType(ogrFldTypeMap),true);
	map<int,int>::iterator iter;
	iter = ogrFldTypeMap.find(ogrType);
	if (iter != ogrFldTypeMap.end())
	{
		smtType = iter->second;
	}
}

void OGRFeaTypeToSmtFeaType(long ogrType,long &smtType)
{
	static	map<int,int>	ogrFeaTypeMap;
	static bool init = (InitOGRFeaTypeToSmtFeaType(ogrFeaTypeMap),true);
	map<int,int>::iterator iter;
	iter = ogrFeaTypeMap.find(ogrType);
	if (iter != ogrFeaTypeMap.end())
	{
		smtType = iter->second;
	}
}

bool CopyOGRFeaToSmtFea(OGRFeature *pOGRFea,SmtFeature  *pSmtFea)
{
	if (NULL == pOGRFea || NULL == pSmtFea)
		return false;

	return (CopyOGRGeomToSmtGeom(pOGRFea,pSmtFea) && CopyOGRAttToSmtAtt(pOGRFea,pSmtFea));
}

bool CopyOGRGeomToSmtGeom(OGRFeature *pOGRFea,SmtFeature  *pSmtFea)
{
	OGRGeometry *pORGGeom;
	if ((pORGGeom = pOGRFea->GetGeometryRef()) == NULL)
		return false;

	switch(wkbFlatten(pORGGeom->getGeometryType()))
	{
	case wkbPoint:
		{
			OGRPoint *poPoint = (OGRPoint *) pORGGeom;
			SmtPoint *pPoint = new SmtPoint(poPoint->getX(), poPoint->getY() );
			pSmtFea->SetFeatureType(SmtFeatureType::SmtFtDot);
			pSmtFea->SetGeometryDirectly(pPoint);
			pSmtFea->SetStyle("DefPointStyle");
		}
		break;
	case wkbLineString:
		{
			OGRLineString *poLineString = (OGRLineString *) pORGGeom;
			SmtLineString *pLineString = new SmtLineString();
			int nPoints;
			nPoints = poLineString->getNumPoints();
			pLineString->SetNumPoints(nPoints);
			for (int i = 0; i < nPoints ;i ++ )
			{
				pLineString->SetPoint(i,poLineString->getX(i),poLineString->getY(i));
			}

			pSmtFea->SetFeatureType(SmtFeatureType::SmtFtCurve);
			pSmtFea->SetStyle("DefLineStyle");
			pSmtFea->SetGeometryDirectly(pLineString);
		}
		break;
	case wkbPolygon:
		{
			OGRPolygon *poPolygon = (OGRPolygon *)pORGGeom;
			OGRLinearRing *poLinearRing = poPolygon->getExteriorRing();

			SmtLinearRing *pLinearRing = new SmtLinearRing();
			int nPoints;
			nPoints = poLinearRing->getNumPoints();
			pLinearRing->SetNumPoints(nPoints);
			for (int i = 0; i < nPoints ;i ++ )
			{
				pLinearRing->SetPoint(i,poLinearRing->getX(i),poLinearRing->getY(i));
			}

			pLinearRing->CloseRings();

			SmtPolygon *pPolygon = new SmtPolygon();
			pPolygon->AddRingDirectly(pLinearRing);

			pSmtFea->SetFeatureType(SmtFeatureType::SmtFtSurface);
			pSmtFea->SetStyle("DefRegionStyle");
			pSmtFea->SetGeometryDirectly(pPolygon);
		}
		break;

	default:
		return false;

		break;
	}

	SmtStyle *pStyle = pSmtFea->GetStyle();
	if (pStyle)
	{
		SmtPenDesc &penDes = pStyle->GetPenDesc();
		penDes.lPenColor = GetRandomColor();

		SmtBrushDesc &brushDes = pStyle->GetBrushDesc();
		brushDes.lBrushColor = GetRandomColor();

		//penDes.lPenColor = GetInterpColor(nCurveID,20,255,0,177,0,255,0);
	}

	return true;
}

bool	CopyOGRAttToSmtAtt(OGRFeature *pOGRFea,SmtFeature  *pSmtFea)
{
	int iCount = pOGRFea->GetFieldCount();
	for (int i = 0; i < iCount; i++)
	{
		OGRFieldDefn * pFldDefn = pOGRFea->GetFieldDefnRef(i);
		switch (pFldDefn->GetType())
		{
		case OFTInteger:
			{
				pSmtFea->SetFieldValue(i,pOGRFea->GetFieldAsInteger(i));
			}
			break;
		case OFTReal:
			{
				pSmtFea->SetFieldValue(i,pOGRFea->GetFieldAsDouble(i));
			}
			break;
		case OFTString:
		case OFTWideString:
			{
				pSmtFea->SetFieldValue(i,pOGRFea->GetFieldAsString(i));
			}
			break;
		}
	}

	return true;
}