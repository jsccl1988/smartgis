#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtMultiPoint::SmtMultiPoint()
	{

	}

	SmtMultiPoint::~SmtMultiPoint()
	{

	}

	// Non standard (Geometry).
	const char *SmtMultiPoint::GetGeometryName() const
	{
		return "MULTIPOINT";
	}

	SmtGeometryType SmtMultiPoint::GetGeometryType() const
	{
		return GTMultiPoint;
	}

	SmtGeometry *SmtMultiPoint::Clone() const
	{
		SmtMultiPoint     *poNewGC;

		poNewGC = new SmtMultiPoint;

		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			poNewGC->AddGeometry( GetGeometryRef(i) );
		}

		return poNewGC;
	}

	void SmtMultiPoint::GetEnvelope(Envelope * psEnvelope ) const
	{
		SmtPoint *pPoint =  NULL;

		for( int i = 0; i < GetNumGeometries(); i++ )
		{
			pPoint = (SmtPoint *)GetGeometryRef(i);
			psEnvelope->Merge(pPoint->GetX(),pPoint->GetY());
		}
	}

	// Non standard
	int SmtMultiPoint::AddGeometryDirectly( SmtGeometry * poNewGeom)
	{
		if( poNewGeom->GetGeometryType() != GTPoint )
			return SMT_ERR_UNSUPPORTED_GEOTYPE;

		return SmtGeometryCollection::AddGeometryDirectly( poNewGeom );
	}

	long SmtMultiPoint::Relationship( const SmtGeometry *pOther,float fMargin) const
	{
		Envelope env;
		this->GetEnvelope(&env);

		long lRet;
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				SmtPoint *pPoint = (SmtPoint *)pOther;
				if (env.Contains(pPoint->GetX(),pPoint->GetY()))
				{
					lRet = SS_Overlaps;
				}
				else 
					lRet = SS_Disjoint;
			}
			break;
		case GTLineString:
		case GTPolygon:
		case GTLinearRing:
		case GTSpline:
			{
				Envelope envOther;
				pOther->GetEnvelope(&envOther);

				if (env.Contains(envOther))
					lRet = SS_Contains;
				else if(env.Intersects(envOther))
					lRet = SS_Intersects;
				else
					lRet = SS_Disjoint;
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
}