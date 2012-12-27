#include "geo_geometry.h"

using namespace Smt_Core;

namespace Smt_Geo
{
	SmtMultiPolygon::SmtMultiPolygon()
	{

	}

	SmtMultiPolygon::~SmtMultiPolygon()
	{

	}

	// Non standard (Geometry).
	const char *SmtMultiPolygon::GetGeometryName() const
	{
        return "MULTIPOLYGON";
	}

	SmtGeometryType SmtMultiPolygon::GetGeometryType() const
	{
        return GTMultiPolygon;
	}

	SmtGeometry *SmtMultiPolygon::Clone() const
	{
		SmtMultiPolygon     *poNewGC;

		poNewGC = new SmtMultiPolygon;
	
		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			poNewGC->AddGeometry( GetGeometryRef(i) );
		}

		return poNewGC;
	}

	// Non standard
	int SmtMultiPolygon::AddGeometryDirectly( SmtGeometry * poNewGeom)
	{
		if( poNewGeom->GetGeometryType() != GTPolygon )
			return SMT_ERR_UNSUPPORTED_GEOTYPE;

		return SmtGeometryCollection::AddGeometryDirectly( poNewGeom );
	}

	double SmtMultiPolygon:: GetArea() const
	{
		double dfArea = 0.0;
		int iPoly;

		for( iPoly = 0; iPoly < GetNumGeometries(); iPoly++ )
		{
			SmtPolygon *poPoly = (SmtPolygon *) GetGeometryRef( iPoly );

			dfArea += poPoly->GetArea();
		}

		return dfArea;
	}
}