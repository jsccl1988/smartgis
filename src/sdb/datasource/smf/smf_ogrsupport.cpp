#include "smf_ogrsupport.h"
#include "api.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_GIS;

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

	if (!sdb::datasource::copy_ogr_feature_to_smt(pOGRFea, pSmtFea))
		return false;

	if (pSmtFea->GetStyle() == NULL)
	{
		switch (pSmtFea->GetFeatureType())
		{
		case SmtFtDot:
		case SmtFtAnno:
			pSmtFea->SetStyle("DefPointStyle");
			break;
		case SmtFtCurve:
			pSmtFea->SetStyle("DefLineStyle");
			break;
		case SmtFtSurface:
			pSmtFea->SetStyle("DefRegionStyle");
			break;
		default:
			break;
		}
	}

	SmtStyle *pStyle = pSmtFea->GetStyle();
	if (pStyle)
	{
		SmtPenDesc &penDes = pStyle->GetPenDesc();
		penDes.lPenColor = GetRandomColor();

		SmtBrushDesc &brushDes = pStyle->GetBrushDesc();
		brushDes.lBrushColor = GetRandomColor();
	}

	return true;
}