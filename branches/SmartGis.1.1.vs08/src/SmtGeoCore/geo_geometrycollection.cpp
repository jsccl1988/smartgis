#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtGeometryCollection::SmtGeometryCollection()
	{
		m_nGeomCount = 0;
		m_pGeoms = NULL;
	}

	SmtGeometryCollection::~SmtGeometryCollection()
	{
        Empty();
	}

	// Non standard (Geometry).
	const char *SmtGeometryCollection::GetGeometryName() const
	{
        return "GEOMETRYCOLLECTION";
	}

	SmtGeometryType SmtGeometryCollection::GetGeometryType() const
	{
        return GTGeometryCollection;
	}

	SmtGeometry *SmtGeometryCollection::Clone() const
	{
		SmtGeometryCollection       *poNewGC;

		poNewGC = new SmtGeometryCollection;
		for( int i = 0; i < m_nGeomCount; i++ )
		{
			poNewGC->AddGeometry( m_pGeoms[i] );
		}

		return poNewGC;
	}

	void SmtGeometryCollection::Empty()
	{
		if (NULL != m_pGeoms)
		{
			for (int i = 0;i < m_nGeomCount;i++)
			{
				SMT_SAFE_DELETE(m_pGeoms[i])
			}

			free(m_pGeoms);
		}

		m_nGeomCount = 0;
	}

	bool  SmtGeometryCollection::IsEmpty() const
	{
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
			if (m_pGeoms[iGeom]->IsEmpty() == false)
				return false;
		return true;
	}

	double SmtGeometryCollection::GetArea() const
	{
		double dfArea = 0.0;
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
		{
			SmtGeometry* geom = m_pGeoms[iGeom];
			switch(geom->GetGeometryType())
			{
			case GTPolygon:
				dfArea += ((SmtPolygon *) geom)->GetArea();
				break;

			case GTMultiPolygon:
				dfArea += ((SmtMultiPolygon *) geom)->GetArea();
				break;

			case GTLinearRing:
			case GTLineString:
				/* This test below is required to filter out wkbLineString geometries
				* not being of type of wkbLinearRing.
				*/
				if( strcmp( ((SmtGeometry*) geom)->GetGeometryName(), "LINEARRING" ) == 0)
				{
					dfArea += ((SmtLinearRing *) geom)->GetArea();
				}
				break;

			case GTGeometryCollection:
				dfArea +=((SmtGeometryCollection *) geom)->GetArea();
				break;

			default:
				break;
			}
		}

		return dfArea;
	}

	// IGeometry methods
	int SmtGeometryCollection::GetDimension() const
	{
        return 2;
	}

	void SmtGeometryCollection::GetEnvelope( Envelope * psEnvelope ) const
	{
		Envelope         oGeomEnv;

		if( m_nGeomCount == 0 )
			return;

		m_pGeoms[0]->GetEnvelope( psEnvelope );

		for( int iGeom = 1; iGeom < m_nGeomCount; iGeom++ )
		{
			m_pGeoms[iGeom]->GetEnvelope( &oGeomEnv );

			if( psEnvelope->MinX > oGeomEnv.MinX )
				psEnvelope->MinX = oGeomEnv.MinX;
			if( psEnvelope->MinY > oGeomEnv.MinY )
				psEnvelope->MinY = oGeomEnv.MinY;
			if( psEnvelope->MaxX < oGeomEnv.MaxX )
				psEnvelope->MaxX = oGeomEnv.MaxX;
			if( psEnvelope->MaxY < oGeomEnv.MaxY )
				psEnvelope->MaxY = oGeomEnv.MaxY;
		}
	}

	// IGeometryCollection
	int SmtGeometryCollection::GetNumGeometries() const
	{
        return m_nGeomCount;
	}

	SmtGeometry *SmtGeometryCollection::GetGeometryRef( int i)
	{
		if( i < 0 || i >= m_nGeomCount )
			return NULL;
		else
			return m_pGeoms[i];
	}

	const SmtGeometry *SmtGeometryCollection::GetGeometryRef( int i ) const
	{
		if( i < 0 || i >= m_nGeomCount )
			return NULL;
		else
			return m_pGeoms[i];
	}

	// ISpatialRelation
	bool  SmtGeometryCollection::Equals( const SmtGeometry * poOther) const
	{
		SmtGeometryCollection *poOGC = (SmtGeometryCollection *) poOther;

		if( poOGC == this )
			return true;

		if( poOther->GetGeometryType() != GetGeometryType() )
			return false;

		if( GetNumGeometries() != poOGC->GetNumGeometries() )
			return false;

		// we should eventually test the SRS.

		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
		{
			if( !GetGeometryRef(iGeom)->Equals(poOGC->GetGeometryRef(iGeom)) )
				return false;
		}

		return true;
	}

	// Non standard
	void SmtGeometryCollection::SetCoordinateDimension( int nDimension )
	{
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
			m_pGeoms[iGeom]->SetCoordinateDimension( nDimension );

		SmtGeometry::SetCoordinateDimension( nDimension );
	}

	int SmtGeometryCollection::AddGeometry( const SmtGeometry * poNewGeom)
	{
		SmtGeometry *poClone = poNewGeom->Clone();

		int eErr;

		eErr = AddGeometryDirectly( poClone );
		if( eErr != SMT_ERR_NONE )
			delete poClone;

		return eErr;
	}

	int SmtGeometryCollection::AddGeometryDirectly( SmtGeometry * poNewGeom)
	{
		m_pGeoms = (SmtGeometry **) realloc( m_pGeoms,sizeof(void*) * (m_nGeomCount+1) );

		m_pGeoms[m_nGeomCount] = poNewGeom;

		m_nGeomCount++;

		return SMT_ERR_NONE;
	}

	int SmtGeometryCollection::RemoveGeometry( int iGeom, int bDelete )
	{
		if( iGeom < -1 || iGeom >= m_nGeomCount )
			return SMT_ERR_FAILURE;

		// Special case.
		if( iGeom == -1 )
		{
			while( m_nGeomCount > 0 )
				RemoveGeometry( m_nGeomCount-1, bDelete );
			return SMT_ERR_NONE;
		}

		if( bDelete )
			SMT_SAFE_DELETE(m_pGeoms[iGeom]);

		memmove( m_pGeoms + iGeom, m_pGeoms + iGeom + 1, sizeof(void*) * (m_nGeomCount-iGeom-1) );

		m_nGeomCount--;

		return SMT_ERR_NONE;
	}

	void SmtGeometryCollection::CloseRings()
	{
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
		{
			if( m_pGeoms[iGeom]->GetGeometryType() == GTPolygon )
				((SmtPolygon *) m_pGeoms[iGeom])->CloseRings();
		}
	}

}