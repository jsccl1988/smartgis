#include <math.h>

#include "smt_api.h"
#include "geo_3dgeometry.h"

using namespace Smt_Core;
namespace Smt_3DGeo
{
	Smt3DLinearRing::Smt3DLinearRing()
	{

	}

	Smt3DLinearRing::Smt3DLinearRing( Smt3DLinearRing * poLr)
	{
		SetNumPoints( poLr->GetNumPoints() );
		memcpy( m_pPoints, poLr->m_pPoints,sizeof(Raw3DPoint) * GetNumPoints() );
	}

	Smt3DLinearRing::~Smt3DLinearRing()
	{

	}

	// Non standard.
	const char *Smt3DLinearRing::GetGeometryName() const
	{
       return "3DLINEARRING";
	}

	Smt3DGeometry *Smt3DLinearRing::Clone() const
	{
		Smt3DLinearRing       *poNewLinearRing;

		poNewLinearRing = new Smt3DLinearRing();
		poNewLinearRing->SetPoints( m_nPointCount, m_pPoints);

		return poNewLinearRing;
	}

	bool Smt3DLinearRing::IsClockwise() const
	{
		int    i, v, next;
		double  dx0, dy0, dx1, dy1, crossproduct; 

		if( m_nPointCount < 2 )
			return true;

		/* Find the lowest rightmost vertex */
		v = 0;
		for ( i = 1; i < m_nPointCount - 1; i++ )
		{
			/* => v < end */
			if ( m_pPoints[i].y< m_pPoints[v].y ||
				( m_pPoints[i].y== m_pPoints[v].y &&
				m_pPoints[i].x > m_pPoints[v].x ) )
			{
				v = i;
			}
		}

		/* Vertices may be duplicate, we have to go to nearest different in each direction */
		/* preceding */
		next = v - 1;
		while ( 1 )
		{
			if ( next < 0 ) 
			{
				next = m_nPointCount - 1 - 1; 
			}

			if( !IsEqual(m_pPoints[next].x, m_pPoints[v].x, dEPSILON) 
				|| !IsEqual(m_pPoints[next].y, m_pPoints[v].y, dEPSILON) )
			{
				break;
			}

			if ( next == v ) /* So we cannot get into endless loop */
			{
				break;
			}

			next--;
		}

		dx0 = m_pPoints[next].x - m_pPoints[v].x;
		dy0 = m_pPoints[next].y - m_pPoints[v].y;


		/* following */
		next = v + 1;
		while ( 1 )
		{
			if ( next >= m_nPointCount - 1 ) 
			{
				next = 0; 
			}

			if ( !IsEqual(m_pPoints[next].x, m_pPoints[v].x, dEPSILON) 
				|| !IsEqual(m_pPoints[next].y, m_pPoints[v].y, dEPSILON) )
			{
				break;
			}

			if ( next == v ) /* So we cannot get into endless loop */
			{
				break;
			}

			next++;
		}

		dx1 = m_pPoints[next].x - m_pPoints[v].x;
		dy1 = m_pPoints[next].y - m_pPoints[v].y;

		crossproduct = dx1 * dy0 - dx0 * dy1;

		if ( crossproduct > 0 )      /* CCW */
			return FALSE;
		else if ( crossproduct < 0 )  /* CW */
			return TRUE;

		/* ok, this is a degenerate case : the extent of the polygon is less than EPSILON */
		/* Try with Green Formula as a fallback, but this is not a guarantee */
		/* as we'll probably be affected by numerical instabilities */

		double dfSum = m_pPoints[0].x * (m_pPoints[1].y - m_pPoints[m_nPointCount-1].y);

		for (i=1; i<m_nPointCount-1; i++) {
			dfSum += m_pPoints[i].x * (m_pPoints[i+1].y - m_pPoints[i-1].y);
		}

		dfSum += m_pPoints[m_nPointCount-1].x * (m_pPoints[0].y - m_pPoints[m_nPointCount-2].y);

		return dfSum < 0;
	}

	void Smt3DLinearRing::ReverseWindingOrder()
	{
		int pos = 0; 
		Smt3DPoint tempPoint; 

		for( int i = 0; i < m_nPointCount / 2; i++ ) 
		{ 
			GetPoint( i, &tempPoint ); 
			pos = m_nPointCount - i - 1; 
			SetPoint( i, GetX(pos), GetY(pos) ,GetZ(pos)); 
			SetPoint( pos, tempPoint.GetX(), tempPoint.GetY(),tempPoint.GetZ()); 
		} 
	}

	void Smt3DLinearRing::CloseRings()
	{
		if( m_nPointCount < 2 )
			return;

		if( GetX(0) != GetX(m_nPointCount-1) || GetY(0) != GetY(m_nPointCount-1)|| GetZ(0) != GetZ(m_nPointCount-1))
			AddPoint( GetX(0), GetY(0),GetZ(0));
	}

	bool Smt3DLinearRing::IsPointInRing(const Smt3DPoint* poPoint, bool bTestAabb) const
	{
		if ( NULL == poPoint )
		{
			return false;
		}

		const int iNumPoints = GetNumPoints();

		// Simple validation
		if ( iNumPoints < 4 )
			return 0;

		const double dfTestX = poPoint->GetX();
		const double dfTestY = poPoint->GetY();
		const double dfTestZ = poPoint->GetZ();

		// Fast test if point is inside extent of the ring
		if (bTestAabb)
		{
			Aabb extent;
			GetAabb(&extent);
			if ( !extent.Intersects(Vector4(dfTestX,dfTestY,dfTestZ)))
			{
				return false;
			}
		}

		// For every point p in ring,
		// test if ray starting from given point crosses segment (p - 1, p)
		int iNumCrossings = 0;

		for ( int iPoint = 1; iPoint < iNumPoints; iPoint++ ) 
		{
			const int iPointPrev = iPoint - 1;

			const double x1 = GetX(iPoint) - dfTestX;
			const double y1 = GetY(iPoint) - dfTestY;
			const double z1 = GetZ(iPoint) - dfTestZ;

			const double x2 = GetX(iPointPrev) - dfTestX;
			const double y2 = GetY(iPointPrev) - dfTestY;
			const double z2 = GetZ(iPointPrev) - dfTestZ;


			if( ( ( y1 > 0 ) && ( y2 <= 0 ) ) || ( ( y2 > 0 ) && ( y1 <= 0 ) ) || ( ( z2 > 0 ) && ( z1 <= 0 ) )) 
			{
				// Check if ray intersects with segment of the ring
				const double dfIntersection = ( x1 * y2 - x2 * y1 ) / (y2 - y1);
				if ( 0.0 < dfIntersection )
				{
					// Count intersections
					iNumCrossings++;
				}
			}
		}

		// If iNumCrossings number is even, given point is outside the ring,
		// when the crossings number is odd, the point is inside the ring.
		return ( ( iNumCrossings % 2 ) == 1 ? true : false );
	}

	bool Smt3DLinearRing::IsPointOnRingBoundary(const Smt3DPoint* poPoint, bool bTestAabb ) const
	{
		if ( NULL == poPoint )
		{
			return 0;
		}

		const int iNumPoints = GetNumPoints();

		// Simple validation
		if ( iNumPoints < 4 )
			return 0;

		const double dfTestX = poPoint->GetX();
		const double dfTestY = poPoint->GetY();
		const double dfTestZ = poPoint->GetZ();


		// Fast test if point is inside extent of the ring
		Aabb extent;
		GetAabb(&extent);
		if ( !extent.Intersects(Vector4(dfTestX,dfTestY,dfTestZ)))
		{
			return 0;
		}

		for ( int iPoint = 1; iPoint < iNumPoints; iPoint++ ) 
		{
			const int iPointPrev = iPoint - 1;

			const double x1 = GetX(iPoint) - dfTestX;
			const double y1 = GetY(iPoint) - dfTestY;
			const double z1 = GetZ(iPoint) - dfTestZ;


			const double x2 = GetX(iPointPrev) - dfTestX;
			const double y2 = GetY(iPointPrev) - dfTestY;
			const double z2 = GetZ(iPointPrev) - dfTestZ;


			/* If iPoint and iPointPrev are the same, go on */
			if (x1 == x2 && y1 == y2 && z1 == z2)
			{
				continue;
			}

			/* If the point is on the segment, return immediatly */
			/* FIXME? If the test point is not exactly identical to one of */
			/* the vertices of the ring, but somewhere on a segment, there's */
			/* little chance that we get 0. So that should be tested against some epsilon */
			if ( x1 * y2 - x2 * y1 == 0 )
			{
				return true;
			}
		}

		return false;
	}
}