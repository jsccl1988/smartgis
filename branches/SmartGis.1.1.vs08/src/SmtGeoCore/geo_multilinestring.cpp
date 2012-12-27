#include "geo_geometry.h"

using namespace Smt_Core;

namespace Smt_Geo
{
	SmtMultiLineString::SmtMultiLineString()
	{

	}

	SmtMultiLineString::~SmtMultiLineString()
	{

	}

	// Non standard (Geometry).
	const char *SmtMultiLineString::GetGeometryName() const
	{
		 return "MULTILINESTRING";
	}

	SmtGeometryType SmtMultiLineString::GetGeometryType() const
	{
		return GTMultiLineString;
	}

	SmtGeometry *SmtMultiLineString::Clone() const
	{
		SmtMultiLineString     *poNewGC;

		poNewGC = new SmtMultiLineString;

		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			poNewGC->AddGeometry( GetGeometryRef(i) );
		}

		return poNewGC;
	}

	// Non standard
	int SmtMultiLineString::AddGeometryDirectly( SmtGeometry * poNewGeom)
	{
		if( poNewGeom->GetGeometryType() != GTLineString )
			return SMT_ERR_UNSUPPORTED_GEOTYPE;

		return SmtGeometryCollection::AddGeometryDirectly( poNewGeom );
	}

}