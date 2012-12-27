#include "geo_3dgeometry.h"
using namespace Smt_Core;

namespace Smt_3DGeo
{
	Smt3DSurface::Smt3DSurface():m_pTriangles(NULL)
		,m_nTraingleCount(0)
		,m_pPoints(NULL)
		,m_nPointCount(0)

	{
		Empty();
	}

	Smt3DSurface::~Smt3DSurface()
	{
		Empty();
	}

	// Geometry
	int Smt3DSurface::GetDimension() const
	{
		return 0;
	}

	const char *Smt3DSurface::GetGeometryName() const
	{
		return "3DSURFACE";
	}

	Smt3DGeometryType Smt3DSurface::GetGeometryType() const
	{
		return GT3DSurface;
	}

	Smt3DGeometry *Smt3DSurface::Clone() const
	{
	/*	if (IsEmpty())
		{
			return NULL;
		}*/
		
		Smt3DSurface    *poNewSurf = new Smt3DSurface();
		poNewSurf->SetCoordinateDimension( m_nCoordDimension);
	
		//clone points 
		int nPoints = m_nPointCount;

		Smt3DPoint * pPoints = new Smt3DPoint[nPoints];
		for (int i = 0; i < nPoints;i++ )
		{
			pPoints[i].SetX(m_pPoints[i].x);
			pPoints[i].SetY(m_pPoints[i].y);
			pPoints[i].SetZ(m_pPoints[i].z);
		}
		
		poNewSurf->AddPointCollection(pPoints,nPoints);

		//clone triangle
		poNewSurf->AddTriangleCollection(m_pTriangles,m_nTraingleCount);

		SMT_SAFE_DELETE_A(pPoints);
		
		return poNewSurf;
	}

	void Smt3DSurface::Empty()
	{
		SMT_SAFE_DELETE_A(m_pPoints);
		SMT_SAFE_DELETE_A(m_pTriangles);
		
		m_nTraingleCount = 0;

		m_nCoordDimension = 0;
	}

	void Smt3DSurface::GetAabb( Aabb * psAabb ) const
	{
		if (psAabb == NULL)
		{
			return;
		}

		for( int iPoint = 0; iPoint < m_nPointCount; iPoint++ )
		{
			psAabb->Merge(m_pPoints[iPoint].x,m_pPoints[iPoint].y,m_pPoints[iPoint].z);
		}
	}

	bool  Smt3DSurface::IsEmpty() const
	{
		return m_nCoordDimension == 0;
	}

	// Surface Interface
	double Smt3DSurface::GetArea() const
	{
		double dfArea = 0.0;

		return dfArea;
	}

	int Smt3DSurface::Centroid( Smt3DPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	int Smt3DSurface::PointOnSurface( Smt3DPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	// Non standard
	void Smt3DSurface::SetCoordinateDimension( int nDimension )
	{
		m_nCoordDimension = nDimension;
	}

	// SpatialRelation
	bool Smt3DSurface::Equals( const Smt3DGeometry * poGeo) const
	{
		Smt3DSurface    *poOSurf = (Smt3DSurface *) poGeo;

		if( poOSurf== this )
			return true;

		if( poOSurf->GetGeometryType() != GetGeometryType() )
			return false;

	   return false;
	}

	//////////////////////////////////////////////////////////////////////////
	Smt3DTriangle	Smt3DSurface::GetTriangle(int iIndex) const
	{
		if( iIndex < 0 || iIndex >= m_nTraingleCount )
			return Smt3DTriangle();
		else
			return m_pTriangles[iIndex];
	}

	Smt3DPoint	Smt3DSurface::GetPoint(int iIndex) const
	{
		Smt3DPoint rawPoint;
		if( iIndex > -1 || iIndex < m_nPointCount )
		{
			rawPoint.SetX( m_pPoints[iIndex].x );
			rawPoint.SetY( m_pPoints[iIndex].y );
			rawPoint.SetZ( m_pPoints[iIndex].z );
		}

		return rawPoint;
	}

	//////////////////////////////////////////////////////////////////////////

	int Smt3DSurface::AddPoint( Smt3DPoint * poPoint)
	{
		m_pPoints = (Raw3DPoint *) realloc( m_pPoints,sizeof(Raw3DPoint) * (m_nPointCount+1) );
		m_pPoints[m_nPointCount].x = poPoint->GetX();
		m_pPoints[m_nPointCount].y = poPoint->GetY();
		m_pPoints[m_nPointCount].z = poPoint->GetZ();
		m_nPointCount++;

		return SMT_ERR_NONE;
	}

	int Smt3DSurface::AddPointCollection( Smt3DPoint*poPoints,int nPoints)
	{
		if( NULL == poPoints || nPoints < 1)
			return SMT_ERR_FAILURE;

		m_pPoints = (Raw3DPoint *) realloc( m_pPoints,sizeof(Raw3DPoint) * (m_nPointCount+nPoints) );

		for (int i = 0; i < nPoints;i++)
		{
			m_pPoints[m_nPointCount+i].x = poPoints[i].GetX();
			m_pPoints[m_nPointCount+i].y = poPoints[i].GetY();
			m_pPoints[m_nPointCount+i].z = poPoints[i].GetZ();
		}

		m_nPointCount += nPoints;

		return SMT_ERR_NONE;
	}

	int Smt3DSurface::AddPointCollection( dbf3DPoint*pPoints,int nPoints)
	{
		if( NULL == pPoints || nPoints < 1)
			return SMT_ERR_FAILURE;

		m_pPoints = (Raw3DPoint *) realloc( m_pPoints,sizeof(Raw3DPoint) * (m_nPointCount+nPoints) );

		/*for (int i = 0; i < nPoints;i++)
		{
			m_pPoints[m_nPointCount+i].x = pPoints[i].x;
			m_pPoints[m_nPointCount+i].y = pPoints[i].y;
			m_pPoints[m_nPointCount+i].z = pPoints[i].z;
		}*/

		memcpy(m_pPoints+m_nPointCount,pPoints,sizeof(dbf3DPoint)*nPoints);

		m_nPointCount += nPoints;

		return SMT_ERR_NONE;
	}

	int Smt3DSurface::AddTriangle( Smt3DTriangle *poNewTri )
	{
		Smt3DTriangle *poNewTri1 = new  Smt3DTriangle;

		memcpy(poNewTri1,poNewTri,sizeof(Smt3DTriangle));

		m_pTriangles = (Smt3DTriangle *) realloc( m_pTriangles,sizeof(Smt3DTriangle) * (m_nTraingleCount+1) );

		m_pTriangles[m_nTraingleCount] = *poNewTri;
		m_nTraingleCount++;

		return SMT_ERR_NONE;
	}


	int Smt3DSurface::AddTriangleCollection( Smt3DTriangle*poTris,int nTris)
	{
		if( NULL == poTris || nTris < 1)
			return SMT_ERR_FAILURE;

		m_pTriangles = (Smt3DTriangle *) realloc( m_pTriangles,sizeof(Smt3DTriangle) * (m_nTraingleCount+nTris) );

		for (int i = 0; i < nTris;i++)
		{
			m_pTriangles[m_nTraingleCount+i] = poTris[i];
		}

		m_nTraingleCount += nTris;

		return SMT_ERR_NONE;
	}

	int Smt3DSurface::RemoveTriangle(int iIndex)
	{
		if( iIndex < -1 || iIndex >= m_nTraingleCount )
			return SMT_ERR_FAILURE;

		// Special case.
		if( iIndex == -1 )
		{
			while( m_nTraingleCount > 0 )
				RemoveTriangle( m_nTraingleCount-1);
			return SMT_ERR_NONE;
		}

		m_pTriangles[iIndex].bDelete = true;

		return SMT_ERR_NONE;
	}
}