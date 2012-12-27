#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtGrid::SmtGrid()
	{
		m_pNodeBuf = NULL;
		m_nRow = 0;    
		m_nCol = 0;
	}

	SmtGrid::SmtGrid(int nRow, int nCol)
	{
		m_pNodeBuf = NULL;
		m_nRow = 0;    
		m_nCol = 0;
		
		SetSize(nRow,nCol);

		m_nCoordDimension = 2;
	}

	SmtGrid::~SmtGrid()
	{
        Empty();
	}

	// Geometry
	int SmtGrid::GetDimension() const
	{
		return 2;
	}

	const char *SmtGrid::GetGeometryName() const
	{
		return "GRID";
	}

	SmtGeometryType SmtGrid::GetGeometryType() const
	{
		return GTGrid;
	}

	SmtGeometry *SmtGrid::Clone() const
	{
		SmtGrid    *poNewGrid = new SmtGrid(m_nRow,m_nCol);

		Matrix2D<RawPoint>  *pNodeBuf = poNewGrid->GetGridNodeBuf();
		for (int i = 0; i < m_nRow; i ++)
			for (int j = 0; j < m_nCol; j++)
			{
				RawPoint rawPt = m_pNodeBuf->GetElement(i,j);
				pNodeBuf->SetElement(rawPt,i,j);
			}

		poNewGrid->SetCoordinateDimension( m_nCoordDimension );

		return poNewGrid;
	}

	void SmtGrid::Empty()
	{
		SMT_SAFE_DELETE(m_pNodeBuf);
		m_nRow = m_nCol = 0.0;

		m_nCoordDimension = 0;
	}

	void SmtGrid::GetEnvelope( Envelope * psEnvelope ) const
	{
		for (int i = 0; i < m_nRow; i ++)
		{
			for (int j = 0; j < m_nCol; j ++)
			{
				psEnvelope->Merge(m_pNodeBuf->GetElement(i,j).x,m_pNodeBuf->GetElement(i,j).y);
			}
		}
	}

	bool  SmtGrid::IsEmpty() const
	{
		return m_nCoordDimension == 0;
	}


	// Non standard
	void SmtGrid::SetCoordinateDimension( int nDimension )
	{
		m_nCoordDimension = nDimension;
	}

	// SpatialRelation
	bool SmtGrid::Equals( const SmtGeometry * poGeo) const
	{
		SmtGrid    *poOGrid = (SmtGrid *) poGeo;

		if( poOGrid == this )
			return true;

		if( poGeo->GetGeometryType() != GetGeometryType() )
			return false;

		// we should eventually test the SRS.

		int nRow,nCol;
		poOGrid->GetSize(nRow,nCol);

		if (nRow != m_nRow || nCol != m_nCol)
			return false;
		
		Matrix2D<RawPoint>  *pNodeBuf = poOGrid->GetGridNodeBuf();
		for (int i = 0; i < m_nRow; i ++)
		{
			for (int j = 0; j < m_nCol; j++)
			{
				RawPoint rawPt = m_pNodeBuf->GetElement(i,j);
				RawPoint rawPt1 = pNodeBuf->GetElement(i,j);

				if ( rawPt1.x != rawPt.x|| rawPt1.y != rawPt.y)
					return false;
			}
		}
		
		return true;
	}

	void SmtGrid::SetSize(int nRow, int nCol)
	{
		//先释放原有空间
		Empty();

		//重置参数
		m_nRow = nRow;
		m_nCol = nCol;
		
		//申请空间
		m_pNodeBuf = new Matrix2D<RawPoint>(m_nRow,m_nCol);
	}
	//////////////////////////////////////////////////////////////////////////
	//重置网格大小
	void SmtGrid::ReSize(int nRow, int nCol)
	{
		if(nRow == m_nRow && nCol == m_nCol) 
			return;

		SetSize(nRow,nCol);
	}

	void SmtGrid::GetSize(int &nRow, int &nCol) const
	{
		nRow = m_nRow;
		nCol = m_nCol;
	}
}