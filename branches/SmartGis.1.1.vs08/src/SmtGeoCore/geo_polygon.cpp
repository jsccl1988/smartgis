#include <math.h>

#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtPolygon::SmtPolygon()
	{
		m_nRingCount = 0;
		m_pRings = NULL;
	}

	SmtPolygon::~SmtPolygon()
	{
        Empty();
	}

	// Non standard (Geometry).
	const char * SmtPolygon::GetGeometryName() const
	{
        return "POLYGON";
	}

	SmtGeometryType SmtPolygon::GetGeometryType() const
	{
        return GTPolygon;
	}


	SmtGeometry *SmtPolygon::Clone() const
	{
		SmtPolygon  *poNewPolygon;

		poNewPolygon = new SmtPolygon;

		for( int i = 0; i < m_nRingCount; i++ )
		{
			poNewPolygon->AddRing( m_pRings[i] );
		}

		return poNewPolygon;
	}

	void SmtPolygon::Empty()
	{
		if (NULL != m_pRings)
		{
			for (int i = 0;i < m_nRingCount;i++)
			{
				SMT_SAFE_DELETE(m_pRings[i])
			}

			free(m_pRings);
		}

		m_nRingCount = 0;
	}

	bool SmtPolygon::IsEmpty() const
	{
		for( int iRing = 0; iRing < m_nRingCount; iRing++ )
			if (m_pRings[iRing]->IsEmpty() == false)
				return false;
		return true;
	}

	// Surface Interface
	double SmtPolygon::GetArea() const
	{
		double dfArea = 0.0;

		if( GetExteriorRing() != NULL )
		{
			int iRing;

			dfArea = GetExteriorRing()->GetArea();

			for( iRing = 0; iRing < GetNumInteriorRings(); iRing++ )
				dfArea -= GetInteriorRing( iRing )->GetArea();
		}

		return dfArea;
	}

	int SmtPolygon::Centroid( SmtPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	int SmtPolygon::PointOnSurface( SmtPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	// Geometry
	int  SmtPolygon::GetDimension() const
	{
       return 2;
	}

	void SmtPolygon::GetEnvelope( Envelope * psEnvelope ) const
	{
		Envelope         oRingEnv;

		if( m_nRingCount == 0 )
			return;

		m_pRings[0]->GetEnvelope( psEnvelope );

		for( int iRing = 1; iRing < m_nRingCount; iRing++ )
		{
			m_pRings[iRing]->GetEnvelope( &oRingEnv );

			if( psEnvelope->MinX > oRingEnv.MinX )
				psEnvelope->MinX = oRingEnv.MinX;
			if( psEnvelope->MinY > oRingEnv.MinY )
				psEnvelope->MinY = oRingEnv.MinY;
			if( psEnvelope->MaxX < oRingEnv.MaxX )
				psEnvelope->MaxX = oRingEnv.MaxX;
			if( psEnvelope->MaxY < oRingEnv.MaxY )
				psEnvelope->MaxY = oRingEnv.MaxY;
		}
	}

	// SpatialRelation
	bool SmtPolygon::Equals( const SmtGeometry * poOther) const
	{
		SmtPolygon *poOPoly = (SmtPolygon *) poOther;

		if( poOPoly == this )
			return TRUE;

		if( poOther->GetGeometryType() != GetGeometryType() )
			return FALSE;

		if( GetNumInteriorRings() != poOPoly->GetNumInteriorRings() )
			return FALSE;

		if( GetExteriorRing() == NULL && poOPoly->GetExteriorRing() == NULL )
			/* ok */;
		else if( GetExteriorRing() == NULL || poOPoly->GetExteriorRing() == NULL )
			return FALSE;
		else if( !GetExteriorRing()->Equals( poOPoly->GetExteriorRing() ) )
			return FALSE;

		// we should eventually test the SRS.

		for( int iRing = 0; iRing < GetNumInteriorRings(); iRing++ )
		{
			if( !GetInteriorRing(iRing)->Equals(poOPoly->GetInteriorRing(iRing)) )
				return FALSE;
		}

		return TRUE;
	}

	//
	long SmtPolygon::Relationship( const SmtGeometry *pOther,float fMargin) const
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
					else
					{
						const SmtLinearRing *pOutRing = GetExteriorRing();
						lRet = pOutRing->Relationship(pOther,fMargin);

						if (SS_Contains == lRet)
						{
							int nInterRNum = GetNumInteriorRings();
							for (int  i = 0; i < nInterRNum;i++)
							{
								const SmtLinearRing *pInterRing = GetInteriorRing(i);
								if (SS_Disjoint != pInterRing->Relationship(pOther,fMargin))
								{
									lRet = SS_Disjoint;
								}
							}
						}
					}
				}
				break;
			case GTLineString:
				{
					lRet = SS_Unkown;
				}
				break;
			case GTPolygon:
				{
					lRet = SS_Unkown;
				}
				break;
		}

		return lRet;
	}

	//
	double SmtPolygon::Distance( const SmtGeometry *pOther ) const
	{
		switch (pOther->GetGeometryType())
		{
		case GTPoint:
			{
				return SMT_C_INVALID_DBF_VALUE;
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
			break;
		}

		return SMT_C_INVALID_DBF_VALUE;

	}

	// Non standard
	void SmtPolygon::SetCoordinateDimension( int nDimension )
	{
		for( int iRing = 0; iRing < m_nRingCount; iRing++ )
			m_pRings[iRing]->SetCoordinateDimension( nDimension );

		SmtGeometry::SetCoordinateDimension( nDimension );
	}


	void SmtPolygon::AddRing( SmtLinearRing * poNewRing)
	{
		m_pRings = (SmtLinearRing **) realloc( m_pRings,sizeof(void*) * (m_nRingCount+1));
		m_pRings[m_nRingCount] = new SmtLinearRing( poNewRing );
		m_nRingCount++;
	}

	void SmtPolygon::AddRingDirectly( SmtLinearRing * poNewRing)
	{
		m_pRings = (SmtLinearRing **) realloc( m_pRings,sizeof(void*) * (m_nRingCount+1));
		m_pRings[m_nRingCount] = poNewRing;
		m_nRingCount++;
	}

	SmtLinearRing *SmtPolygon::GetExteriorRing()
	{
		if( m_nRingCount > 0 )
			return m_pRings[0];
		else
			return NULL;
	}

	const SmtLinearRing *SmtPolygon::GetExteriorRing() const
	{
		if( m_nRingCount > 0 )
			return m_pRings[0];
		else
			return NULL;
	}

	int  SmtPolygon::GetNumInteriorRings() const
	{
		if( m_nRingCount > 0 )
			return m_nRingCount-1;
		else
			return 0;
	}

	SmtLinearRing *SmtPolygon::GetInteriorRing( int iRing )
	{
		if( iRing < 0 || iRing >= m_nRingCount-1 )
			return NULL;
		else
			return m_pRings[iRing+1];
	}

	const SmtLinearRing *SmtPolygon::GetInteriorRing( int iRing) const
	{
		if( iRing < 0 || iRing >= m_nRingCount-1 )
			return NULL;
		else
			return m_pRings[iRing+1];
	}

	bool SmtPolygon::IsPointOnSurface( const SmtPoint *pt ) const
	{
		if ( NULL == pt)
			return 0;

		for( int iRing = 0; iRing < m_nRingCount; iRing++ )
		{
			if ( m_pRings[iRing]->IsPointInRing(pt) )
			{
				return 1;
			}
		}

		return 0;
	}

	void SmtPolygon::CloseRings()
	{
		for( int iRing = 0; iRing < m_nRingCount; iRing++ )
			m_pRings[iRing]->CloseRings();
	}

}