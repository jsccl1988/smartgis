#include "geo_3Dgeometry.h"
using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DGeometry::Smt3DGeometry()
	{
		m_nCoordDimension = 3;
	}

	Smt3DGeometry::~Smt3DGeometry()
	{

	}

	void Smt3DGeometry::SetCoordinateDimension( int nNewDimension )
	{
		m_nCoordDimension = nNewDimension;
	}

	int  Smt3DGeometry::GetCoordinateDimension() const 
	{
		return m_nCoordDimension;
	}

	bool  Smt3DGeometry::IsValid() const
	{
		return false;
	}

	bool  Smt3DGeometry::IsSimple() const
	{
		return false;
	}

	bool  Smt3DGeometry::IsRing() const
	{
		return false;
	}

	// SpatialRelation
	bool  Smt3DGeometry::Intersects(const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	bool  Smt3DGeometry::Disjoint( const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	bool  Smt3DGeometry::Touches( const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	bool  Smt3DGeometry::Crosses( const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	bool  Smt3DGeometry::Within( const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	bool  Smt3DGeometry::Contains( const Smt3DGeometry * poGeo) const
	{
		return false;
	}

	Smt3DGeometry *Smt3DGeometry::GetBoundary() const
	{
		return NULL;
	}

	double    Smt3DGeometry::Distance( const Smt3DGeometry * poGeo) const
	{
		return -1.0;
	}

	Smt3DGeometry *Smt3DGeometry::Buffer( double dfDist, int nQuadSegs ) const
	{
		return NULL;
	}

	Smt3DGeometry *Smt3DGeometry::Intersection( const Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	Smt3DGeometry *Smt3DGeometry::Union( const Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	Smt3DGeometry *Smt3DGeometry::Difference( const Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	Smt3DGeometry *Smt3DGeometry::SymmetricDifference( const Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	// backward compatibility methods. 
	bool  Smt3DGeometry::Intersect( Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return false;
		}

		return NULL;
	}

	bool  Smt3DGeometry::Equal( Smt3DGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return false;
	}
}