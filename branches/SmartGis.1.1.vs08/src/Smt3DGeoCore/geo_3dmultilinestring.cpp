#include "geo_3dgeometry.h"

using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DMultiLineString::Smt3DMultiLineString()
	{

	}

	Smt3DMultiLineString::~Smt3DMultiLineString()
	{

	}

	// Non standard (Geometry).
	const char *Smt3DMultiLineString::GetGeometryName() const
	{
		 return "3DMULTILINESTRING";
	}

	Smt3DGeometryType Smt3DMultiLineString::GetGeometryType() const
	{
		return GT3DMultiLineString;
	}

	Smt3DGeometry *Smt3DMultiLineString::Clone() const
	{
		Smt3DMultiLineString     *poNewGC;

		poNewGC = new Smt3DMultiLineString;

		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			poNewGC->AddGeometry( GetGeometryRef(i) );
		}

		return poNewGC;
	}

	// Non standard
	int Smt3DMultiLineString::AddGeometryDirectly( Smt3DGeometry * poNewGeom)
	{
		if( poNewGeom->GetGeometryType() != GT3DLineString )
			return SMT_ERR_UNSUPPORTED_GEOTYPE;

		return Smt3DGeometryCollection::AddGeometryDirectly( poNewGeom );
	}

}