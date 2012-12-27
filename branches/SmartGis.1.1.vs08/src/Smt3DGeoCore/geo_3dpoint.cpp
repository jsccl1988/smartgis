#include "geo_3dgeometry.h"
using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DPoint::Smt3DPoint()
	{
		Empty();
	}

	Smt3DPoint::Smt3DPoint( double xIn, double yIn ,double zIn )
	{
		point3D.x = xIn;
		point3D.y = yIn;
		point3D.z = zIn;

		m_nCoordDimension = 3;
	}

	Smt3DPoint::Smt3DPoint(Raw3DPoint &pt3d)
	{
		point3D = pt3d;
	}

	Smt3DPoint::~Smt3DPoint()
	{

	}

	// Geometry
	int Smt3DPoint::GetDimension() const
	{
		return 0;
	}

	const char *Smt3DPoint::GetGeometryName() const
	{
		return "3dbfPoint";
	}

	Smt3DGeometryType Smt3DPoint::GetGeometryType() const
	{
		return GT3DPoint;
	}

	Smt3DGeometry *Smt3DPoint::Clone() const
	{
		Smt3DPoint    *poNewPoint = new Smt3DPoint(point3D.x,point3D.y,point3D.z);
		poNewPoint->SetCoordinateDimension( m_nCoordDimension );
		return poNewPoint;
	}

	void Smt3DPoint::Empty()
	{
		point3D.x = point3D.y = point3D.z =.0;
		m_nCoordDimension = 0;
	}

	void Smt3DPoint::GetAabb( Aabb * psAabb ) const
	{
		psAabb->Merge(point3D.x,point3D.y,point3D.z);
	}

	bool  Smt3DPoint::IsEmpty() const
	{
		return m_nCoordDimension == 0;
	}


	// Non standard
	void Smt3DPoint::SetCoordinateDimension( int nDimension )
	{
		m_nCoordDimension = nDimension;
	}

	// SpatialRelation
	bool Smt3DPoint::Equals( const Smt3DGeometry * poGeo) const
	{
		Smt3DPoint    *poOPoint = (Smt3DPoint *) poGeo;

		if( poOPoint== this )
			return true;

		if( poGeo->GetGeometryType() != GetGeometryType() )
			return false;

		// we should eventually test the SRS.

		if( poOPoint->GetX() != GetX()
			|| poOPoint->GetY() != GetY()
			|| poOPoint->GetZ() != GetZ()
			)
			return false;
		else
			return true;
	}
}