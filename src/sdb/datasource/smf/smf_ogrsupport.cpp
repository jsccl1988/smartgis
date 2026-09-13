#include "sdb/datasource/smf/smf_ogrsupport.h"
#include "base/core/api.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
using namespace geo;
using namespace base;
using namespace sdb;

void init_ogr_fld_type_map(map<int, int>& ogrFldTypeMap)
{
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTInteger,SmtInteger));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTReal,SmtReal));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTInteger,SmtByte));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTString,SmtString));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTTime,SmtTime));
	ogrFldTypeMap.insert(map<int,int>::value_type(OFTDateTime,SmtDateTime));
}

void init_ogr_fea_type_map(map<int, int>& ogrFeaTypeMap)
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

void ogr_fld_type_to_smt_fld_type(long ogrType, long& smtType)
{
	static map<int, int> ogrFldTypeMap;
	static bool init = (init_ogr_fld_type_map(ogrFldTypeMap), true);
	map<int,int>::iterator iter;
	iter = ogrFldTypeMap.find(ogrType);
	if (iter != ogrFldTypeMap.end())
	{
		smtType = iter->second;
	}
}

void ogr_fea_type_to_smt_fea_type(long ogrType, long& smtType)
{
	static map<int, int> ogrFeaTypeMap;
	static bool init = (init_ogr_fea_type_map(ogrFeaTypeMap), true);
	map<int,int>::iterator iter;
	iter = ogrFeaTypeMap.find(ogrType);
	if (iter != ogrFeaTypeMap.end())
	{
		smtType = iter->second;
	}
}

bool copy_ogr_fea_to_smt_fea(OGRFeature* pOGRFea, SmtFeature* pSmtFea)
{
	if (NULL == pOGRFea || NULL == pSmtFea)
		return false;

	if (!sdb::datasource::copy_ogr_feature_to_smt(pOGRFea, pSmtFea))
		return false;

	if (pSmtFea->get_style() == NULL)
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

	SmtStyle *pStyle = pSmtFea->get_style();
	if (pStyle)
	{
		SmtPenDesc &penDes = pStyle->get_pen_desc();
		penDes.lPenColor = GetRandomColor();

		SmtBrushDesc &brushDes = pStyle->get_brush_desc();
		brushDes.lBrushColor = GetRandomColor();
	}

	return true;
}