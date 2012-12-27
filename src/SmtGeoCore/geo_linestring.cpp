#include <assert.h>
#include <math.h>

#include "geo_geometry.h"
#include "geo_api.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtLineString::SmtLineString()
	{
		m_nPointCount = 0;
		m_pPoints = NULL;
	}

	SmtLineString::SmtLineString(SmtLineString *pLS)
	{
		SetNumPoints( pLS->GetNumPoints() );
		memcpy( m_pPoints, pLS->m_pPoints,sizeof(RawPoint) * GetNumPoints() );
	}

	SmtLineString::~SmtLineString()
	{
        if (NULL != m_pPoints)
        {
			free(m_pPoints);
			m_nPointCount = 0;
        }
	}

	// Geometry interface
	int SmtLineString::GetDimension() const
	{
        return 2;
	}

	SmtGeometryType SmtLineString::GetGeometryType() const
	{
		return GTLineString;
	}

	const char *SmtLineString::GetGeometryName() const
	{
		return "LINESTRING";
	}

	//////////////////////////////////////////////////////////////////////////
	SmtGeometry *SmtLineString::Clone() const
	{
		SmtLineString       *poNewLineString;

		poNewLineString = new SmtLineString();

		poNewLineString->SetPoints( m_nPointCount, m_pPoints);
		poNewLineString->SetCoordinateDimension( GetCoordinateDimension() );

		return poNewLineString;
	}
	
	void SmtLineString::Empty()
	{
        SetNumPoints( 0 );
	}

	void SmtLineString::GetEnvelope( Envelope * psEnvelope ) const
	{
		if (NULL == psEnvelope)
			return;
		double      dfMinX, dfMinY, dfMaxX, dfMaxY;

		if( m_nPointCount == 0 )
			return;

		dfMinX = dfMaxX = m_pPoints[0].x;
		dfMinY = dfMaxY = m_pPoints[0].y;

		for( int iPoint = 1; iPoint < m_nPointCount; iPoint++ )
		{
			if( dfMaxX < m_pPoints[iPoint].x )
				dfMaxX = m_pPoints[iPoint].x;
			if( dfMaxY < m_pPoints[iPoint].y )
				dfMaxY = m_pPoints[iPoint].y;
			if( dfMinX > m_pPoints[iPoint].x )
				dfMinX = m_pPoints[iPoint].x;
			if( dfMinY > m_pPoints[iPoint].y )
				dfMinY = m_pPoints[iPoint].y;
		}

		psEnvelope->MinX = dfMinX;
		psEnvelope->MaxX = dfMaxX;
		psEnvelope->MinY = dfMinY;
		psEnvelope->MaxY = dfMaxY;
	}

	bool SmtLineString::IsEmpty() const
	{
       return (m_nPointCount == 0);
	}

	// Curve methods
	double SmtLineString::GetLength() const
	{
		double      dfLength = 0;
		int         i;

		for( i = 0; i < m_nPointCount-1; i++ )
		{
			double      dfDeltaX, dfDeltaY;

			dfDeltaX = m_pPoints[i+1].x - m_pPoints[i].x;
			dfDeltaY = m_pPoints[i+1].y - m_pPoints[i].y;
			dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);
		}

		return dfLength;
	}

	void SmtLineString::StartPoint(SmtPoint *poPoint) const
	{
        GetPoint( 0, poPoint );
	}

	void SmtLineString::EndPoint(SmtPoint *poPoint) const
	{
        GetPoint( m_nPointCount - 1, poPoint );
	}

	void SmtLineString::Value( double dfDistance, SmtPoint * poPoint ) const
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
			double      dfDeltaX, dfDeltaY, dfSegLength;

			dfDeltaX = m_pPoints[i+1].x - m_pPoints[i].x;
			dfDeltaY = m_pPoints[i+1].y - m_pPoints[i].y;
			dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

			if (dfSegLength > 0)
			{
				if( (dfLength <= dfDistance) && 
					((dfLength + dfSegLength) >= dfDistance) )
				{
					double      dfRatio;

					dfRatio = (dfDistance - dfLength) / dfSegLength;

					poPoint->SetX( m_pPoints[i].x * (1 - dfRatio)+ m_pPoints[i+1].x * dfRatio );
					poPoint->SetY( m_pPoints[i].y * (1 - dfRatio)+ m_pPoints[i+1].y * dfRatio );

					return;
				}

				dfLength += dfSegLength;
			}
		}

		EndPoint( poPoint );
	}

	// LineString methods
	void SmtLineString::GetPoint( int i, SmtPoint * poPoint) const
	{
		assert( i >= 0 );
		assert( i < m_nPointCount );
		assert( poPoint != NULL );

		poPoint->SetX( m_pPoints[i].x );
		poPoint->SetY( m_pPoints[i].y );
	}

	void SmtLineString::GetPoint( int i, RawPoint *rawPoint ) const
	{
		assert( i >= 0 );
		assert( i < m_nPointCount );
		assert( poPoint != NULL );

		rawPoint->x = m_pPoints[i].x ;
		rawPoint->y = m_pPoints[i].y ;
	}

	// SpatialRelation
	bool SmtLineString::Equals( const SmtGeometry * poOther) const
	{
		SmtLineString       *poOLine = (SmtLineString *) poOther;

		if( poOLine == this )
			return true;

		if( poOther->GetGeometryType() != GetGeometryType() )
			return false;

		// we should eventually test the SRS.
		if( GetNumPoints() != poOLine->GetNumPoints() )
			return false;

		for( int iPoint = 0; iPoint < GetNumPoints(); iPoint++ )
		{
			if( GetX(iPoint) != poOLine->GetX(iPoint)|| 
				GetY(iPoint) != poOLine->GetY(iPoint))
				return false;
		}

		return true;
	}

	//
	long SmtLineString::Relationship( const SmtGeometry *pOther,float fMargin) const
	{
		long lRet = SS_Unkown;
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				Envelope env,oenv;
				GetEnvelope(&env);
				pOther->GetEnvelope(&oenv);

				if (!env.Intersects(oenv))
				{
					lRet = SS_Disjoint;
				}
				else if (Distance(pOther) < fMargin)
				{
					lRet = SS_Overlaps;
				}
				else if (m_pPoints[m_nPointCount - 1].x == m_pPoints[0].x && m_pPoints[m_nPointCount - 1].y == m_pPoints[0].y)
				{
					if (m_nPointCount < 3) 
						return SS_Unkown;

					SmtPoint *pPoint = (SmtPoint *)pOther;
					RawPoint rawPoint(pPoint->GetX(),pPoint->GetY());

					long flag = SmtPointToPolygon_New1(rawPoint,m_pPoints,m_nPointCount);
					if (flag == -1)
						lRet = SS_Contains;
					else if (flag == 0)
						lRet = SS_Overlaps;
					else
						lRet = SS_Disjoint;
				}
			}
			break;
		case GTLineString:
		case GTLinearRing:
		case GTSpline:
		case GTPolygon:
			{
				Envelope env,oenv;
				GetEnvelope(&env);
				pOther->GetEnvelope(&oenv);

				if (!env.Intersects(oenv))
				{
					lRet = SS_Disjoint;
				}
				else if (m_pPoints[m_nPointCount - 1].x == m_pPoints[0].x && m_pPoints[m_nPointCount - 1].y == m_pPoints[0].y &&
					     env.Contains(oenv))
				{
					lRet = SS_Contains;
				}
				else
				{
					lRet = SS_Intersects;
				}
			}
			break;
		}

		return lRet;
	}

	//
	double SmtLineString::Distance( const SmtGeometry *pOther ) const
	{
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				int indexPre = 0,indexNext = 0;
				SmtPoint *pPoint = (SmtPoint *)pOther;
				RawPoint rawPoint(pPoint->GetX(),pPoint->GetY());
				return SmtDistance_New(rawPoint,m_pPoints,m_nPointCount,indexPre,indexNext);
			}
			break;
		case GTLineString:
			{
				return SMT_C_INVALID_DBF_VALUE;
			}
			break;
		case GTPolygon:
			{
				return SMT_C_INVALID_DBF_VALUE;
			}
			break;
		default:
			return SMT_C_INVALID_DBF_VALUE;
		}

		return SMT_C_INVALID_DBF_VALUE;
	}

	// non standard.
	void SmtLineString::SetCoordinateDimension( int nDimension )
	{
        m_nCoordDimension = nDimension;
	}

	void SmtLineString::SetNumPoints( int nNewPointCount)
	{
		if( nNewPointCount == 0 )
		{
			free( m_pPoints );
			m_nPointCount = 0;
			return;
		}

		if( nNewPointCount > m_nPointCount )
		{
			m_pPoints = (RawPoint *)realloc(m_pPoints, sizeof(RawPoint) * nNewPointCount);
			assert( m_pPoints != NULL );
			memset( m_pPoints + m_nPointCount,0, sizeof(RawPoint) * (nNewPointCount - m_nPointCount) );
		}

		m_nPointCount = nNewPointCount;
	}

	void SmtLineString::SetPoint( int iPoint, SmtPoint *poPoint )
	{
         SetPoint( iPoint, poPoint->GetX(), poPoint->GetY());
	}
	
	void SmtLineString::SetPoint( int iPoint, double xIn, double yIn)
	{
		if( iPoint >= m_nPointCount )
			SetNumPoints( iPoint+1 );

		m_pPoints[iPoint].x = xIn;
		m_pPoints[iPoint].y = yIn;
	}

	void SmtLineString::SetPoints( int nPointsIn, RawPoint *pPointsIn)
	{
		if(NULL == pPointsIn)
			return;

		SetNumPoints( nPointsIn );
		memcpy( m_pPoints, pPointsIn, sizeof(RawPoint) * nPointsIn);
	}

	void SmtLineString::SetPoints( int nPointsIn, double * padfX, double * padfY)
	{
		SetNumPoints( nPointsIn );

		int i;

		for( i = 0; i < nPointsIn; i++ )
		{
			m_pPoints[i].x = padfX[i];
			m_pPoints[i].y = padfY[i];
		}
	}

	void SmtLineString::AddPoint( SmtPoint * poPoint)
	{
       SetPoint( m_nPointCount, poPoint->GetX(), poPoint->GetY());
	}

	void SmtLineString::AddPoint( double xIn, double yIn)
	{
       SetPoint( m_nPointCount, xIn,yIn);
	}

	void SmtLineString::GetPoints( RawPoint *poPoints) const
	{
		if(NULL == poPoints)
			return;
         memcpy( poPoints, m_pPoints, sizeof(RawPoint) * m_nPointCount );
	}

	void SmtLineString::AddSubLineString( const SmtLineString *pSubLinestring, int nStartVertex, int nEndVertex  )
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
			memcpy( m_pPoints + nOldbfPoints, pSubLinestring->m_pPoints + nStartVertex, sizeof(RawPoint) * nPointsToAdd );
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
			}
		}
	}
}