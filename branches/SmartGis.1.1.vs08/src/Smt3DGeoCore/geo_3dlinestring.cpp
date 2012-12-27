
#include <assert.h>
#include <math.h>

#include "geo_3dgeometry.h"

using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DLineString::Smt3DLineString()
	{
		m_nPointCount = 0;
		m_pPoints = NULL;
	}

	Smt3DLineString::~Smt3DLineString()
	{
        if (NULL != m_pPoints)
        {
			free(m_pPoints);
			m_nPointCount = 0;
        }
        
	}

	// Geometry interface
	int Smt3DLineString::GetDimension() const
	{
        return 3;
	}

	Smt3DGeometryType Smt3DLineString::GetGeometryType() const
	{
		return GT3DLineString;
	}

	const char *Smt3DLineString::GetGeometryName() const
	{
		return "3DLINESTRING";
	}

	//////////////////////////////////////////////////////////////////////////
	Smt3DGeometry *Smt3DLineString::Clone() const
	{
		Smt3DLineString       *poNewLineString;

		poNewLineString = new Smt3DLineString();

		poNewLineString->SetPoints( m_nPointCount, m_pPoints);
		poNewLineString->SetCoordinateDimension( GetCoordinateDimension() );

		return poNewLineString;
	}
	
	void Smt3DLineString::Empty()
	{
        SetNumPoints( 0 );
	}

	void Smt3DLineString::GetAabb( Aabb * psAabb ) const
	{
		if (psAabb == NULL)
		{
			return;
		}
		
		for( int iPoint = 0; iPoint < GetNumPoints(); iPoint++ )
		{
			 psAabb->Merge(m_pPoints[iPoint].x,m_pPoints[iPoint].y,m_pPoints[iPoint].z);
		}
	}

	bool Smt3DLineString::IsEmpty() const
	{
       return (m_nPointCount == 0);
	}

	// Curve methods
	double Smt3DLineString::GetLength() const
	{
		double      dfLength = 0;
		int         i;

		for( i = 0; i < m_nPointCount-1; i++ )
		{
			double      dfDeltaX, dfDeltaY,dfDeltaZ;

			dfDeltaX = m_pPoints[i+1].x - m_pPoints[i].x;
			dfDeltaY = m_pPoints[i+1].y - m_pPoints[i].y;
			dfDeltaZ = m_pPoints[i+1].z - m_pPoints[i].z;
			dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY + dfDeltaZ*dfDeltaZ );
		}

		return dfLength;
	}

	void Smt3DLineString::StartPoint(Smt3DPoint *poPoint) const
	{
        GetPoint( 0, poPoint );
	}

	void Smt3DLineString::EndPoint(Smt3DPoint *poPoint) const
	{
        GetPoint( m_nPointCount - 1, poPoint );
	}

	void Smt3DLineString::Value( double dfDistance, Smt3DPoint * poPoint ) const
	{
		double      dfLength = 0;
		int         i;

		if( dfDistance < 0 )
		{
			StartPoint( poPoint );
			return;
		}

		for( i = 0; i < m_nPointCount-1; i++ )
		{
			double      dfDeltaX, dfDeltaY, dfDeltaZ,dfSegLength;

			dfDeltaX = m_pPoints[i+1].x - m_pPoints[i].x;
			dfDeltaY = m_pPoints[i+1].y - m_pPoints[i].y;
			dfDeltaZ = m_pPoints[i+1].z - m_pPoints[i].z;
			dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY + dfDeltaZ*dfDeltaZ);

			if (dfSegLength > 0)
			{
				if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >= 
					dfDistance) )
				{
					double      dfRatio;

					dfRatio = (dfDistance - dfLength) / dfSegLength;

					poPoint->SetX( m_pPoints[i].x * (1 - dfRatio)+ m_pPoints[i+1].x * dfRatio );
					poPoint->SetY( m_pPoints[i].y * (1 - dfRatio)+ m_pPoints[i+1].y * dfRatio );
					poPoint->SetZ( m_pPoints[i].z * (1 - dfRatio)+ m_pPoints[i+1].z * dfRatio );
					return;
				}

				dfLength += dfSegLength;
			}
		}

		EndPoint( poPoint );
	}

	// LineString methods
	void Smt3DLineString::GetPoint( int i, Smt3DPoint * poPoint) const
	{
		assert( i >= 0 );
		assert( i < m_nPointCount );
		assert( poPoint != NULL );

		poPoint->SetX( m_pPoints[i].x );
		poPoint->SetY( m_pPoints[i].y );
		poPoint->SetZ( m_pPoints[i].z );
	}
	
	// SpatialRelation
	bool Smt3DLineString::Equals( const Smt3DGeometry * poOther) const
	{
		Smt3DLineString       *poOLine = (Smt3DLineString *) poOther;

		if( poOLine == this )
			return true;

		if( poOther->GetGeometryType() != GetGeometryType() )
			return false;

		// we should eventually test the SRS.

		if( GetNumPoints() != poOLine->GetNumPoints() )
			return false;

		for( int iPoint = 0; iPoint < GetNumPoints(); iPoint++ )
		{
			if( GetX(iPoint) != poOLine->GetX(iPoint)
				|| GetY(iPoint) != poOLine->GetY(iPoint))
				return false;
		}

		return true;
	}

	// non standard.
	void Smt3DLineString::SetCoordinateDimension( int nDimension )
	{
        m_nCoordDimension = nDimension;
	}

	void Smt3DLineString::SetNumPoints( int nNewPointCount)
	{
		if( nNewPointCount == 0 )
		{
			free( m_pPoints );
			m_nPointCount = 0;
			return;
		}

		if( nNewPointCount > m_nPointCount )
		{
			m_pPoints = (Raw3DPoint *)realloc(m_pPoints, sizeof(Raw3DPoint) * nNewPointCount);
			assert( m_pPoints != NULL );
			memset( m_pPoints + m_nPointCount,0, sizeof(Raw3DPoint) * (nNewPointCount - m_nPointCount) );
		}

		m_nPointCount = nNewPointCount;
	}

	void Smt3DLineString::SetPoint( int iPoint, Smt3DPoint *poPoint )
	{
         SetPoint( iPoint, poPoint->GetX(), poPoint->GetY(),poPoint->GetZ());
	}
	
	void Smt3DLineString::SetPoint( int iPoint, double xIn, double yIn,double zIn)
	{
		if( iPoint >= m_nPointCount )
			SetNumPoints( iPoint+1 );

		m_pPoints[iPoint].x = xIn;
		m_pPoints[iPoint].y = yIn;
		m_pPoints[iPoint].z = zIn;

	}

	void Smt3DLineString::SetPoints( int nPointsIn, Raw3DPoint *pPointsIn)
	{
		if(NULL == pPointsIn)
			return;

		SetNumPoints( nPointsIn );
		memcpy( m_pPoints, pPointsIn, sizeof(Raw3DPoint) * nPointsIn);
	}

	void Smt3DLineString::SetPoints( int nPointsIn, double * padfX, double * padfY,double * padfZ)
	{
		SetNumPoints( nPointsIn );

		int i;

		for( i = 0; i < nPointsIn; i++ )
		{
			m_pPoints[i].x = padfX[i];
			m_pPoints[i].y = padfY[i];
			m_pPoints[i].z = padfZ[i];

		}

	}

	void Smt3DLineString::AddPoint( Smt3DPoint * poPoint)
	{
       SetPoint( m_nPointCount, poPoint->GetX(), poPoint->GetY(),poPoint->GetZ());
	}

	void Smt3DLineString::AddPoint( double xIn, double yIn,double zIn)
	{
       SetPoint( m_nPointCount, xIn,yIn,zIn);
	}

	void Smt3DLineString::GetPoints( Raw3DPoint *poPoints) const
	{
		if(NULL == poPoints)
			return;
         memcpy( poPoints, m_pPoints, sizeof(Raw3DPoint) * m_nPointCount );
	}

	void Smt3DLineString::AddSubLineString( const Smt3DLineString *pSubLinestring, int nStartVertex, int nEndVertex  )
	{
		if( nEndVertex == -1 )
			nEndVertex = pSubLinestring->GetNumPoints() - 1;

		if( nStartVertex < 0 || nEndVertex < 0 
			|| nStartVertex >= pSubLinestring->GetNumPoints() 
			|| nEndVertex >= pSubLinestring->GetNumPoints() )
		{
			return;
		}

		/* -------------------------------------------------------------------- */
		/*      Grow this linestring to hold the additional points.             */
		/* -------------------------------------------------------------------- */
		int nOldbfPoints = m_nPointCount;
		int nPointsToAdd = abs(nEndVertex-nStartVertex) + 1;

		SetNumPoints( nPointsToAdd + nOldbfPoints );

		/* -------------------------------------------------------------------- */
		/*      Copy the x/y points - forward copies use memcpy.                */
		/* -------------------------------------------------------------------- */
		if( nEndVertex >= nStartVertex )
		{
			memcpy( m_pPoints + nOldbfPoints, pSubLinestring->m_pPoints + nStartVertex, sizeof(Raw3DPoint) * nPointsToAdd );
		}

		/* -------------------------------------------------------------------- */
		/*      Copy the x/y points - reverse copies done double by double.     */
		/* -------------------------------------------------------------------- */
		else
		{
			int i;

			for( i = 0; i < nPointsToAdd; i++ )
			{
				m_pPoints[i+nOldbfPoints].x = pSubLinestring->m_pPoints[nStartVertex-i].x;
				m_pPoints[i+nOldbfPoints].y = pSubLinestring->m_pPoints[nStartVertex-i].y;
				m_pPoints[i+nOldbfPoints].z = pSubLinestring->m_pPoints[nStartVertex-i].z;

			}
		}
	}
}