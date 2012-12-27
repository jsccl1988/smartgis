#include "geo_3dgeometry.h"

using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DMultiPoint::Smt3DMultiPoint()
	{

	}

	Smt3DMultiPoint::~Smt3DMultiPoint()
	{

	}

	// Non standard (Geometry).
	const char *Smt3DMultiPoint::GetGeometryName() const
	{
		return "3DMULTIPOINT";
	}

	Smt3DGeometryType Smt3DMultiPoint::GetGeometryType() const
	{
		return GT3DMultiPoint;
	}

	Smt3DGeometry *Smt3DMultiPoint::Clone() const
	{
		Smt3DMultiPoint     *poNewGC;

		poNewGC = new Smt3DMultiPoint;

		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			poNewGC->AddGeometry( GetGeometryRef(i) );
		}

		return poNewGC;
	}

	// Non standard
	int Smt3DMultiPoint::AddGeometryDirectly( Smt3DGeometry * poNewGeom)
	{
		if( poNewGeom->GetGeometryType() != GT3DPoint )
			return SMT_ERR_UNSUPPORTED_GEOTYPE;

		return Smt3DGeometryCollection::AddGeometryDirectly( poNewGeom );
	}

}