#include "geo_geometry.h"
using namespace Smt_Core;
using namespace Smt_Base;
namespace Smt_Geo
{
	SmtTin::SmtTin():m_pTriangles(NULL)
		,m_nTraingleCount(0)
		,m_pPoints(NULL)
		,m_nPointCount(0)

	{
		Empty();
	}

	SmtTin::~SmtTin()
	{
		Empty();
	}

	// Geometry
	int SmtTin::GetDimension() const
	{
		return 0;
	}

	const char *SmtTin::GetGeometryName() const
	{
		return "TIN";
	}

	SmtGeometryType SmtTin::GetGeometryType() const
	{
		return GTTin;
	}

	SmtGeometry *SmtTin::Clone() const
	{
		/*if (IsEmpty())
		{
			return NULL;
		}
		*/
		SmtTin    *poNewTin = new SmtTin();
		poNewTin->SetCoordinateDimension( m_nCoordDimension);
	
		//clone points 
		int nPoints = m_nPointCount;

		SmtPoint * pPoints = new SmtPoint[nPoints];
		for (int i = 0; i < nPoints;i++ )
		{
			pPoints[i].SetX(m_pPoints[i].x);
			pPoints[i].SetY(m_pPoints[i].y);
		}
		
		poNewTin->AddPointCollection(pPoints,nPoints);

		//clone triangle
		poNewTin->AddTriangleCollection(m_pTriangles,m_nTraingleCount);

		SMT_SAFE_DELETE_A(pPoints);
		
		return poNewTin;
	}

	void SmtTin::Empty()
	{
		SMT_SAFE_DELETE_A(m_pPoints);
		 
		SMT_SAFE_DELETE_A(m_pTriangles);

		m_nTraingleCount = 0;

		m_nCoordDimension = 0;
	}

	void SmtTin::GetEnvelope(Envelope * psEnvelope ) const
	{
		if (psEnvelope == NULL || m_nPointCount < 1)
			return;

		psEnvelope->MaxX = m_pPoints[0].x;
		psEnvelope->MaxY = m_pPoints[0].y;
		psEnvelope->MinX = m_pPoints[0].x;
		psEnvelope->MinY = m_pPoints[0].y;

		for( int iPoint = 0; iPoint < m_nPointCount; iPoint++ )
			psEnvelope->Merge(m_pPoints[iPoint].x,m_pPoints[iPoint].y);
	}

	bool  SmtTin::IsEmpty() const
	{
		return m_nCoordDimension == 0;
	}

	// Non standard
	void SmtTin::SetCoordinateDimension( int nDimension )
	{
		m_nCoordDimension = nDimension;
	}

	// SpatialRelation
	bool SmtTin::Equals( const SmtGeometry * poGeo) const
	{
		SmtTin    *poTin = (SmtTin *) poGeo;

		if( poTin== this )
			return true;

		if( poTin->GetGeometryType() != GetGeometryType() )
			return false;

	   return false;
	}

	//////////////////////////////////////////////////////////////////////////
	SmtTriangle	SmtTin::GetTriangle(int iIndex) const
	{
		if( iIndex < 0 || iIndex >= m_nTraingleCount )
			return SmtTriangle();
		else
			return m_pTriangles[iIndex];
	}

	SmtPoint	SmtTin::GetPoint(int iIndex) const
	{
		SmtPoint rawPoint;
		if( iIndex > -1 || iIndex < m_nPointCount )
		{
			rawPoint.SetX( m_pPoints[iIndex].x );
			rawPoint.SetY( m_pPoints[iIndex].y );
		}

		return rawPoint;
	}

	//////////////////////////////////////////////////////////////////////////

	int SmtTin::AddPoint( SmtPoint * poPoint)
	{
		m_pPoints = (RawPoint *) realloc( m_pPoints,sizeof(RawPoint) * (m_nPointCount+1) );
		m_pPoints[m_nPointCount].x = poPoint->GetX();
		m_pPoints[m_nPointCount].y = poPoint->GetY();
		m_nPointCount++;

		return SMT_ERR_NONE;
	}

	int SmtTin::AddPointCollection( SmtPoint*poPoint,int nPoints)
	{
		if( NULL == poPoint || nPoints < 1)
			return SMT_ERR_FAILURE;

		m_pPoints = (RawPoint *) realloc( m_pPoints,sizeof(RawPoint) * (m_nPointCount+nPoints) );

		for (int i = 0; i < nPoints;i++)
		{
			m_pPoints[m_nPointCount+i].x = poPoint[i].GetX();
			m_pPoints[m_nPointCount+i].y = poPoint[i].GetY();
		}

		m_nPointCount += nPoints;

		return SMT_ERR_NONE;
	}

	int SmtTin::AddPointCollection(dbfPoint*pPoints,int nPoints)
	{
		if( NULL == pPoints || nPoints < 1)
			return SMT_ERR_FAILURE;

		m_pPoints = (RawPoint *) realloc( m_pPoints,sizeof(RawPoint) * (m_nPointCount+nPoints) );

		for (int i = 0; i < nPoints;i++)
		{
			m_pPoints[m_nPointCount+i].x = pPoints[i].x;
			m_pPoints[m_nPointCount+i].y = pPoints[i].y;
		}

		m_nPointCount += nPoints;

		return SMT_ERR_NONE;
	}

	int SmtTin::AddTriangle( SmtTriangle *poNewTri )
	{
		SmtTriangle *poNewTri1 = new  SmtTriangle;

		memcpy(poNewTri1,poNewTri,sizeof(SmtTriangle));

		m_pTriangles = (SmtTriangle *) realloc( m_pTriangles,sizeof(SmtTriangle) * (m_nTraingleCount+1) );

		m_pTriangles[m_nTraingleCount] = *poNewTri;
		m_nTraingleCount++;

		return SMT_ERR_NONE;
	}


	int SmtTin::AddTriangleCollection( SmtTriangle*poTris,int nTris)
	{
		if( NULL == poTris || nTris < 1)
			return SMT_ERR_FAILURE;

		m_pTriangles = (SmtTriangle *) realloc( m_pTriangles,sizeof(SmtTriangle) * (m_nTraingleCount+nTris) );

		for (int i = 0; i < nTris;i++)
		{
			m_pTriangles[m_nTraingleCount+i] = poTris[i];
		}

		m_nTraingleCount += nTris;

		return SMT_ERR_NONE;
	}

	int SmtTin::RemoveTriangle(int iIndex)
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