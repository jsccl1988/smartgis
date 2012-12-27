#include "geo_geometry.h"
using namespace Smt_Core;

namespace Smt_Geo
{
	SmtGeometry::SmtGeometry()
	{
       m_nCoordDimension = 2;
	}

	SmtGeometry::~SmtGeometry()
	{

	}

	void SmtGeometry::SetCoordinateDimension( int nNewDimension )
	{
		m_nCoordDimension = nNewDimension;
	}

	int  SmtGeometry::GetCoordinateDimension() const 
	{
		return m_nCoordDimension;
	}

	bool  SmtGeometry::IsValid() const
	{
        return false;
	}

	bool  SmtGeometry::IsSimple() const
	{
        return false;
	}

	bool  SmtGeometry::IsRing() const
	{
        return false;
	}
	
	// SpatialRelation
	bool  SmtGeometry::Intersects(const SmtGeometry * poGeo) const
	{
        return false;
	}

	bool  SmtGeometry::Disjoint( const SmtGeometry * poGeo) const
	{
        return false;
	}

	bool  SmtGeometry::Touches( const SmtGeometry * poGeo) const
	{
        return false;
	}
	
	bool  SmtGeometry::Crosses( const SmtGeometry * poGeo) const
	{
        return false;
	}

	bool  SmtGeometry::Within( const SmtGeometry * poGeo) const
	{
        return false;
	}

	bool  SmtGeometry::Contains( const SmtGeometry * poGeo) const
	{
        return false;
	}

	bool  SmtGeometry::Overlaps( const SmtGeometry * poGeo) const
	{
        return false;
	}
	
	SmtGeometry *SmtGeometry::GetBoundary() const
	{
        return NULL;
	}

	double    SmtGeometry::Distance( const SmtGeometry * poGeo) const
	{
        return -1.0;
	}

	SmtGeometry *SmtGeometry::ConvexHull() const
	{
        return NULL;
	}

	SmtGeometry *SmtGeometry::Buffer( double dfDist, int nQuadSegs ) const
	{
         return NULL;
	}

	SmtGeometry *SmtGeometry::Intersection( const SmtGeometry * poGeo) const
	{
        if (NULL == poGeo)
        {
			return NULL;
        }
        
		return NULL;
	}

	SmtGeometry *SmtGeometry::Union( const SmtGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	SmtGeometry *SmtGeometry::Difference( const SmtGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	long SmtGeometry::Relationship( const SmtGeometry *,float fMargin) const
	{
		return -2;
	}

	SmtGeometry *SmtGeometry::SymmetricDifference( const SmtGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return NULL;
	}

	// backward compatibility methods. 
	bool  SmtGeometry::Intersect( SmtGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return false;
		}

		return NULL;
	}

	bool  SmtGeometry::Equal( SmtGeometry * poGeo) const
	{
		if (NULL == poGeo)
		{
			return NULL;
		}

		return false;
	}
}