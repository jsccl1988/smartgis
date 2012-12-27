#include "geo_api.h"
#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtPoint::SmtPoint()
	{
       Empty();
	}

	SmtPoint::SmtPoint( double xIn, double yIn )
	{
        x = xIn;
		y = yIn;
		m_nCoordDimension = 2;
	}

	SmtPoint::~SmtPoint()
	{

	}

	// Geometry
	int SmtPoint::GetDimension() const
	{
       return 0;
	}

	const char *SmtPoint::GetGeometryName() const
	{
		return "POINT";
	}

	SmtGeometryType SmtPoint::GetGeometryType() const
	{
		return GTPoint;
	}

	SmtGeometry *SmtPoint::Clone() const
	{
		SmtPoint    *poNewPoint = new SmtPoint( x, y);
		poNewPoint->SetCoordinateDimension( m_nCoordDimension );
		return poNewPoint;
	}

	void SmtPoint::Empty()
	{
		x = y = 0.0;
		m_nCoordDimension = 0;
	}

	void SmtPoint::GetEnvelope( Envelope * psEnvelope ) const
	{
		psEnvelope->MinX = GetX()-1;
		psEnvelope->MaxX = GetX()+1;
		psEnvelope->MinY = GetY()-1;
		psEnvelope->MaxY = GetY()+1;
	}

	bool  SmtPoint::IsEmpty() const
	{
        return m_nCoordDimension == 0;
	}

	
	// Non standard
	void SmtPoint::SetCoordinateDimension( int nDimension )
	{
         m_nCoordDimension = nDimension;
	}

	// SpatialRelation
	bool SmtPoint::Equals( const SmtGeometry * poGeo) const
	{
		SmtPoint    *poOPoint = (SmtPoint *) poGeo;

		if( poOPoint== this )
			return true;

		if( poGeo->GetGeometryType() != GetGeometryType() )
			return false;

		// we should eventually test the SRS.

		if( poOPoint->GetX() != GetX()
			|| poOPoint->GetY() != GetY()
			)
			return false;
		else
			return true;
	}

	long SmtPoint::Relationship( const SmtGeometry *pOther,float fMargin) const
	{
		long lRet;
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				if ( Distance(pOther) < fMargin)
				{
					lRet = SS_Overlaps;
				}
				else 
					lRet = SS_Disjoint;
			}
			break;
		case GTLineString:
			{
				lRet = pOther->Relationship(this,fMargin);
			}
			break;
		case GTPolygon:
		case GTLinearRing:
		case GTSpline:
			{
				lRet = pOther->Relationship(this,fMargin);
				switch (lRet)
				{
				case SS_Contains:
					lRet = SS_Within;
					break;
				case SS_Disjoint:
					lRet = SS_Disjoint;
					break;
				}
			}
			break;
		default:
			{
				lRet = SS_Unkown;
			}
			break;
		}
		
		return lRet;
	}

	double SmtPoint::Distance( const SmtGeometry * pOther) const
	{
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				SmtPoint *pPoint = (SmtPoint *)pOther;
				return SmtDistance(GetX(),GetY(),pPoint->GetX(),pPoint->GetY());
			}
			break;
		case GTLineString:
			{
				SmtLineString*pLineString = (SmtLineString*)pOther;
				return pLineString->Distance(this);
			}
			break;
		case GTPolygon:
			{
				SmtPolygon*pPoly= (SmtPolygon*)pOther;
				return pPoly->Distance(this);
			}
			break;
		default:
			{
				return SMT_C_INVALID_DBF_VALUE;
			}
			break;
		}

		return SMT_C_INVALID_DBF_VALUE;
	}
}