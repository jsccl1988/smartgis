#include "geo_3dgeometry.h"
using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DGeometryCollection::Smt3DGeometryCollection()
	{
		m_nGeomCount = 0;
		m_pGeoms = NULL;
	}

	Smt3DGeometryCollection::~Smt3DGeometryCollection()
	{
		Empty();
	}

	// Non standard (Geometry).
	const char *Smt3DGeometryCollection::GetGeometryName() const
	{
		return "3DGEOMETRYCOLLECTION";
	}

	Smt3DGeometryType Smt3DGeometryCollection::GetGeometryType() const
	{
		return GT3DGeometryCollection;
	}

	Smt3DGeometry *Smt3DGeometryCollection::Clone() const
	{
		Smt3DGeometryCollection       *poNewGC;

		poNewGC = new Smt3DGeometryCollection;
		for( int i = 0; i < m_nGeomCount; i++ )
		{
			poNewGC->AddGeometry( m_pGeoms[i] );
		}

		return poNewGC;
	}

	void Smt3DGeometryCollection::Empty()
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

	bool  Smt3DGeometryCollection::IsEmpty() const
	{
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
			if (m_pGeoms[iGeom]->IsEmpty() == false)
				return false;
		return true;
	}

	double Smt3DGeometryCollection::GetArea() const
	{
		double dfArea = 0.0;
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
		{
			Smt3DGeometry* geom = m_pGeoms[iGeom];
			switch(geom->GetGeometryType())
			{
			case GT3DSurface:
				dfArea += ((Smt3DSurface *) geom)->GetArea();
				break;

			case GT3DVolum:
				//dfArea += ((Smt3DVolum *) geom)->GetArea();
				break;

			case GT3DGeometryCollection:
				dfArea +=((Smt3DGeometryCollection *) geom)->GetArea();
				break;

			default:
				break;
			}
		}

		return dfArea;
	}

	// IGeometry methods
	int Smt3DGeometryCollection::GetDimension() const
	{
		return 3;
	}

	void Smt3DGeometryCollection::GetAabb( Aabb * psAabb ) const
	{
		Aabb         oGeomAabb;

		if( m_nGeomCount == 0 )
			return;

		m_pGeoms[0]->GetAabb( psAabb );

		for( int iGeom = 1; iGeom < m_nGeomCount; iGeom++ )
		{
			m_pGeoms[iGeom]->GetAabb( &oGeomAabb );
			psAabb->Merge(oGeomAabb);
		}
	}

	// IGeometryCollection
	int Smt3DGeometryCollection::GetNumGeometries() const
	{
		return m_nGeomCount;
	}

	Smt3DGeometry *Smt3DGeometryCollection::GetGeometryRef( int i)
	{
		if( i < 0 || i >= m_nGeomCount )
			return NULL;
		else
			return m_pGeoms[i];
	}

	const Smt3DGeometry *Smt3DGeometryCollection::GetGeometryRef( int i ) const
	{
		if( i < 0 || i >= m_nGeomCount )
			return NULL;
		else
			return m_pGeoms[i];
	}

	// ISpatialRelation
	bool  Smt3DGeometryCollection::Equals( const Smt3DGeometry * poOther) const
	{
		Smt3DGeometryCollection *poOGC = (Smt3DGeometryCollection *) poOther;

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
	void Smt3DGeometryCollection::SetCoordinateDimension( int nDimension )
	{
		for( int iGeom = 0; iGeom < m_nGeomCount; iGeom++ )
			m_pGeoms[iGeom]->SetCoordinateDimension( nDimension );

		Smt3DGeometry::SetCoordinateDimension( nDimension );
	}

	int Smt3DGeometryCollection::AddGeometry( const Smt3DGeometry * poNewGeom)
	{
		if( NULL == poNewGeom )
			return SMT_ERR_FAILURE;

		Smt3DGeometry *poClone = poNewGeom->Clone();

		int eErr;

		eErr = AddGeometryDirectly( poClone );
		if( eErr != SMT_ERR_NONE )
			delete poClone;

		return eErr;
	}

	int Smt3DGeometryCollection::AddGeometrys( const Smt3DGeometry * pGeoms,int nGeoms)
	{
		if( NULL == pGeoms || nGeoms < 1)
			return SMT_ERR_FAILURE;

		m_pGeoms = (Smt3DGeometry **) realloc( m_pGeoms,sizeof(void*) * (m_nGeomCount+nGeoms) );

		for (int i = 0; i < nGeoms;i++)
		{
			m_pGeoms[m_nGeomCount+i] = pGeoms[i].Clone();
		}

		m_nGeomCount += nGeoms;
		
		return SMT_ERR_NONE;
	}

	int Smt3DGeometryCollection::AddGeometrysDirectly(Smt3DGeometry ** pGeoms,int nGeoms)
	{
		if( NULL == pGeoms || nGeoms < 1)
			return SMT_ERR_FAILURE;

		m_pGeoms = (Smt3DGeometry **) realloc( m_pGeoms,sizeof(void*) * (m_nGeomCount+nGeoms) );

		for (int i = 0; i < nGeoms;i++)
		{
			m_pGeoms[m_nGeomCount+i] = pGeoms[i];
		}

		m_nGeomCount += nGeoms;

		return SMT_ERR_NONE;
	}

	int Smt3DGeometryCollection::AddGeometryDirectly( Smt3DGeometry * poNewGeom)
	{
		if( NULL == poNewGeom )
			return SMT_ERR_FAILURE;

		m_pGeoms = (Smt3DGeometry **) realloc( m_pGeoms,sizeof(void*) * (m_nGeomCount+1) );

		m_pGeoms[m_nGeomCount] = poNewGeom;

		m_nGeomCount++;


		return SMT_ERR_NONE;
	}

	int Smt3DGeometryCollection::RemoveGeometry( int iGeom,  bool bDelete)
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
}