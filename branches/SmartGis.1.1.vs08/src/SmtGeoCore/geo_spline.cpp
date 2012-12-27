#include <math.h>

#include "geo_geometry.h"
#include "geo_api.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtSpline::SmtSpline():m_pAnalyticPoints(NULL)
		,m_nAnalyticType(CT_BSPLINE)
		,m_nAnalyticPointNum(0)
	{

	}

	SmtSpline::SmtSpline( SmtSpline * poLr):m_pAnalyticPoints(NULL)
		,m_nAnalyticType(CT_BSPLINE)
		,m_nAnalyticPointNum(0)
	{
		SetNumPoints( poLr->GetNumPoints() );
		memcpy( m_pPoints, poLr->m_pPoints,sizeof(RawPoint) * GetNumPoints() );
	}

	SmtSpline::~SmtSpline()
	{
		SMT_SAFE_DELETE_A(m_pAnalyticPoints);
		m_nAnalyticPointNum = 0;
	}

	// Non standard.
	const char *SmtSpline::GetGeometryName() const
	{
		return "SPLINE";
	}

	SmtGeometryType SmtSpline::GetGeometryType() const
	{
		return GTSpline;
	}

	SmtGeometry *SmtSpline::Clone() const
	{
		SmtSpline   *poNewSpline = new SmtSpline();
		poNewSpline->SetPoints( m_nPointCount, m_pPoints);
		poNewSpline->SetAnalyticType(m_nAnalyticType);
		poNewSpline->CalcAnalyticPoints();

		return poNewSpline;
	}

	void SmtSpline::CalcAnalyticPoints(void)
	{
		//
		SMT_SAFE_DELETE_A(m_pAnalyticPoints);
		m_nAnalyticPointNum = 0;

		//
		//...
		switch (m_nAnalyticType)
		{
		case CT_LAGSPLINE:
			SmtInterpolate_Lugrange(m_pAnalyticPoints,m_nAnalyticPointNum,m_pPoints,m_nPointCount);
			break;
		case CT_BZERSPLINE:
			SmtInterpolate_Geometry(m_pAnalyticPoints,m_nAnalyticPointNum,m_pPoints,m_nPointCount);
			break;
		case CT_BSPLINE:
			SmtInterpolate_BSpline(m_pAnalyticPoints,m_nAnalyticPointNum,m_pPoints,m_nPointCount);
			break;
		case CT_3SPLINE:
			SmtInterpolate_3Spline(m_pAnalyticPoints,m_nAnalyticPointNum,m_pPoints,m_nPointCount);
			break;
		default:
			break;
		}
	}

	void SmtSpline::GetAnalyticPoints( RawPoint *poPoints) const
	{
		if(NULL == poPoints)
			return;

		memcpy( poPoints, m_pAnalyticPoints, sizeof(RawPoint) * m_nAnalyticPointNum );
	}

	void SmtSpline::GetEnvelope( Envelope * psEnvelope ) const
	{
		if (NULL == psEnvelope)
			return;
		double      dfMinX, dfMinY, dfMaxX, dfMaxY;

		if( m_nPointCount == 0 )
			return;

		SmtLineString::GetEnvelope(psEnvelope);

		if (NULL != m_pAnalyticPoints)
		{
			dfMinX = dfMaxX = m_pAnalyticPoints[0].x;
			dfMinY = dfMaxY = m_pAnalyticPoints[0].y;

			for( int iPoint = 1; iPoint < m_nAnalyticPointNum; iPoint++ )
			{
				if( dfMaxX < m_pAnalyticPoints[iPoint].x )
					dfMaxX = m_pAnalyticPoints[iPoint].x;
				if( dfMaxY < m_pAnalyticPoints[iPoint].y )
					dfMaxY = m_pAnalyticPoints[iPoint].y;
				if( dfMinX > m_pAnalyticPoints[iPoint].x )
					dfMinX = m_pAnalyticPoints[iPoint].x;
				if( dfMinY > m_pAnalyticPoints[iPoint].y )
					dfMinY = m_pAnalyticPoints[iPoint].y;
			}

			psEnvelope->Merge(dfMinX,dfMinY);
			psEnvelope->Merge(dfMaxX,dfMaxY);
		}
	}

	// Curve methods
	double SmtSpline::GetLength() const
	{
		double      dfLength = 0;
		int         i;

		if (NULL != m_pAnalyticPoints)
		{
			for( i = 0; i < m_nAnalyticPointNum-1; i++ )
			{
				double      dfDeltaX, dfDeltaY;

				dfDeltaX = m_pAnalyticPoints[i+1].x - m_pAnalyticPoints[i].x;
				dfDeltaY = m_pAnalyticPoints[i+1].y - m_pAnalyticPoints[i].y;
				dfLength += sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);
			}

			return dfLength;
		}
		else
		{
			return SmtLineString::GetLength();
		}
	}

	void SmtSpline::Value( double dfDistance, SmtPoint * poPoint ) const
	{
		double      dfLength = 0;
		int         i;

		if( dfDistance < 0 )
		{
			StartPoint( poPoint );
			return;
		}

		for( i = 0; i < m_nAnalyticPointNum-1; i++ )
		{
			double      dfDeltaX, dfDeltaY, dfSegLength;

			dfDeltaX = m_pAnalyticPoints[i+1].x - m_pAnalyticPoints[i].x;
			dfDeltaY = m_pAnalyticPoints[i+1].y - m_pAnalyticPoints[i].y;
			dfSegLength = sqrt(dfDeltaX*dfDeltaX + dfDeltaY*dfDeltaY);

			if (dfSegLength > 0)
			{
				if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >= 
					dfDistance) )
				{
					double      dfRatio;

					dfRatio = (dfDistance - dfLength) / dfSegLength;

					poPoint->SetX( m_pAnalyticPoints[i].x * (1 - dfRatio)+ m_pAnalyticPoints[i+1].x * dfRatio );
					poPoint->SetY( m_pAnalyticPoints[i].y * (1 - dfRatio)+ m_pAnalyticPoints[i+1].y * dfRatio );

					return;
				}

				dfLength += dfSegLength;
			}
		}

		EndPoint( poPoint );
	}

	//
	long SmtSpline::Relationship( const SmtGeometry *pOther,float fMargin) const
	{
		if (NULL == m_pAnalyticPoints)
			return SmtLineString::Relationship(pOther,fMargin);

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
				else if (m_pAnalyticPoints[m_nAnalyticPointNum - 1].x == m_pPoints[0].x && m_pPoints[m_nAnalyticPointNum - 1].y == m_pPoints[0].y)
				{
					if (m_nPointCount < 3) 
						return SS_Unkown;

					SmtPoint *pPoint = (SmtPoint *)pOther;
					RawPoint rawPoint(pPoint->GetX(),pPoint->GetY());

					long flag = SmtPointToPolygon_New1(rawPoint,m_pAnalyticPoints,m_nAnalyticPointNum);
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
		case GTPolygon:
			{
				Envelope env,oenv;
				GetEnvelope(&env);
				pOther->GetEnvelope(&oenv);

				if (!env.Intersects(oenv))
				{
					lRet = SS_Disjoint;
				}
				else if (m_pAnalyticPoints[m_nAnalyticPointNum - 1].x == m_pAnalyticPoints[0].x && 
					m_pAnalyticPoints[m_nAnalyticPointNum - 1].y == m_pAnalyticPoints[0].y &&
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
	double SmtSpline::Distance( const SmtGeometry *pOther ) const
	{
		if (NULL == m_pAnalyticPoints)
			return SmtLineString::Distance(pOther);

		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				int indexPre = 0,indexNext = 0;
				SmtPoint *pPoint = (SmtPoint *)pOther;
				RawPoint rawPoint(pPoint->GetX(),pPoint->GetY());
				return SmtDistance_New(rawPoint,m_pAnalyticPoints,m_nAnalyticPointNum,indexPre,indexNext);
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
}