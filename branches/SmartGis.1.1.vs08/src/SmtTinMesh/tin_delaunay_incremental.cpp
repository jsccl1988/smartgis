#include "tin_delaunay_incremental.h"
#include <cmath>

namespace Smt_TinMesh
{
	SmtTinDelaunayInc::SmtTinDelaunayInc()
	{
		m_pTinMesh = new TinMesh;
	}

	SmtTinDelaunayInc::~SmtTinDelaunayInc()
	{
		UnInitTinMesh();
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtTinDelaunayInc::SetVertexs(const Vector3 *pVector3Ds,int nCount)
	{
		if (NULL == pVector3Ds || nCount < 1)
			return SMT_ERR_INVALID_PARAM;
		 
		UnInitTinMesh();

		m_pTinMesh->vVers.resize(nCount+3);
		m_pTinMesh->nTriangles = 0;

		m_fMaxX = m_fMinX = pVector3Ds[0].x;
		m_fMaxY = m_fMinY = pVector3Ds[0].y;
		m_fMaxZ = m_fMinZ = pVector3Ds[0].z;

		for (int i = 0; i < nCount;i++)
		{
			m_pTinMesh->vVers[i+3].x = pVector3Ds[i].x;
			m_pTinMesh->vVers[i+3].y = pVector3Ds[i].y;
			m_pTinMesh->vVers[i+3].z = pVector3Ds[i].z;

			m_fMaxX = max(pVector3Ds[i].x,m_fMaxX);
			m_fMaxY = max(pVector3Ds[i].y,m_fMaxY);
			m_fMaxZ = max(pVector3Ds[i].z,m_fMaxZ);

			m_fMinX = min(pVector3Ds[i].x,m_fMinX);
			m_fMinY = min(pVector3Ds[i].y,m_fMinY);
			m_fMinZ = min(pVector3Ds[i].z,m_fMinZ);
		}

		return SMT_ERR_NONE;
	}

	long SmtTinDelaunayInc::SetVertexs(const vector<Vector3> &vVector3Ds)
	{
		if (vVector3Ds.size() < 1)
			return SMT_ERR_INVALID_PARAM;

		UnInitTinMesh();

		m_pTinMesh->vVers.resize(vVector3Ds.size()+3);
		m_pTinMesh->nTriangles = 0;

		m_fMaxX = m_fMinX = vVector3Ds[0].x;
		m_fMaxY = m_fMinY = vVector3Ds[0].y;
		m_fMaxZ = m_fMinZ = vVector3Ds[0].z;

		for (int i = 0; i < vVector3Ds.size();i++)
		{
			m_pTinMesh->vVers[i+3].x = vVector3Ds[i].x;
			m_pTinMesh->vVers[i+3].y = vVector3Ds[i].y;
			m_pTinMesh->vVers[i+3].z = vVector3Ds[i].z;

			m_fMaxX = max(vVector3Ds[i].x,m_fMaxX);
			m_fMaxY = max(vVector3Ds[i].y,m_fMaxY);
			m_fMaxZ = max(vVector3Ds[i].z,m_fMaxZ);

			m_fMinX = min(vVector3Ds[i].x,m_fMinX);
			m_fMinY = min(vVector3Ds[i].y,m_fMinY);
			m_fMinZ = min(vVector3Ds[i].z,m_fMinZ);
		}

		return SMT_ERR_NONE;
	}

	long SmtTinDelaunayInc::CvtTo3DSurf(Smt3DSurface *p3DSurf)
	{
		if (NULL == p3DSurf || NULL == m_pTinMesh)
			return SMT_ERR_INVALID_PARAM;

		Smt3DPoint *p3DPointBuf = new Smt3DPoint[m_pTinMesh->vVers.size()-3];
		for (int i = 0; i < m_pTinMesh->vVers.size()-3;i++)
		{
			p3DPointBuf[i].SetX(m_pTinMesh->vVers[i+3].x);
			p3DPointBuf[i].SetY(m_pTinMesh->vVers[i+3].y);
			p3DPointBuf[i].SetZ(m_pTinMesh->vVers[i+3].z);
		}

		p3DSurf->AddPointCollection(p3DPointBuf,m_pTinMesh->vVers.size()-3);

		if (NULL != m_pTinMesh->pTriArr)
		{
			Smt3DTriangle *p3DTriBuf = new Smt3DTriangle[m_pTinMesh->nTriangles];
			TrianglePtr pTri = m_pTinMesh->pTriArr;
			int i = 0;
			while(pTri != NULL)	
			{
				p3DTriBuf[i].a = pTri->i3-3;
				p3DTriBuf[i].b = pTri->i2-3;
				p3DTriBuf[i].c = pTri->i1-3;
				pTri = pTri ->pNext;
				i++;
			}

			p3DSurf->AddTriangleCollection(p3DTriBuf,m_pTinMesh->nTriangles);
			SMT_SAFE_DELETE_A(p3DTriBuf);
		}

		SMT_SAFE_DELETE_A(p3DPointBuf);

		return SMT_ERR_NONE;
	}

	long SmtTinDelaunayInc::CvtTo2DTin(SmtTin *pTin)
	{
		if (NULL == pTin || NULL == m_pTinMesh)
			return SMT_ERR_INVALID_PARAM;

		SmtPoint	*pPointBuf = new SmtPoint[m_pTinMesh->vVers.size()-3];
		
		for (int i = 0; i < m_pTinMesh->vVers.size()-3;i++)
		{
			pPointBuf[i].SetX(m_pTinMesh->vVers[i+3].x);
			pPointBuf[i].SetY(m_pTinMesh->vVers[i+3].y);
		}

		pTin->AddPointCollection(pPointBuf,m_pTinMesh->vVers.size()-3);

		if (NULL != m_pTinMesh->pTriArr)
		{
			SmtTriangle *pTriBuf = new SmtTriangle[m_pTinMesh->nTriangles];
			TrianglePtr pTri = m_pTinMesh->pTriArr;
			int i = 0;
			while(pTri != NULL)	
			{
				pTriBuf[i].a = pTri->i3-3;
				pTriBuf[i].b = pTri->i2-3;
				pTriBuf[i].c = pTri->i1-3;
				pTri = pTri ->pNext;
				i++;
			}

			pTin->AddTriangleCollection(pTriBuf,m_pTinMesh->nTriangles);

			SMT_SAFE_DELETE_A(pTriBuf);
		}

		SMT_SAFE_DELETE_A(pPointBuf);

		return SMT_ERR_NONE;
	}

	long SmtTinDelaunayInc::TinMeshTo3DSurf(Smt3DSurface *p3DSurf,TinMeshPtr pMesh)
	{
		if (NULL == p3DSurf || NULL == pMesh)
			return SMT_ERR_INVALID_PARAM;

		Smt3DPoint *p3DPointBuf = new Smt3DPoint[pMesh->vVers.size()-3];
		for (int i = 0; i < pMesh->vVers.size()-3;i++)
		{
			p3DPointBuf[i].SetX(pMesh->vVers[i+3].x);
			p3DPointBuf[i].SetY(pMesh->vVers[i+3].z);
			p3DPointBuf[i].SetZ(pMesh->vVers[i+3].y);
		}

		p3DSurf->AddPointCollection(p3DPointBuf,pMesh->vVers.size()-3);

		if (NULL != pMesh->pTriArr)
		{
			Smt3DTriangle *p3DTriBuf = new Smt3DTriangle[pMesh->nTriangles];
			TrianglePtr pTri = pMesh->pTriArr;
			int i = 0;
			while(pTri != NULL)	
			{
				p3DTriBuf[i].a = pTri->i3-3;
				p3DTriBuf[i].b = pTri->i2-3;
				p3DTriBuf[i].c = pTri->i1-3;
				pTri = pTri ->pNext;
				i++;
			}

			p3DSurf->AddTriangleCollection(p3DTriBuf,pMesh->nTriangles);
			SMT_SAFE_DELETE_A(p3DTriBuf);
		}
		
		SMT_SAFE_DELETE_A(p3DPointBuf);
		
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtTinDelaunayInc::UnInitTinMesh(void)
	{
		if (NULL == m_pTinMesh)
			return;

		// free vertices
		m_pTinMesh->vVers.clear();

		// free triangles
		TrianglePtr pTri = m_pTinMesh->pTriArr;
		TrianglePtr pTemp = NULL;
		while (pTri != NULL)
		{
			pTemp = pTri->pNext;
			delete pTri;
			pTri = pTemp;
		}

		m_pTinMesh->pTriArr = NULL;

		EdgePtr pEdge = m_pTinMesh->pEdgeArr;
		EdgePtr pTmp = NULL;
		while (pEdge != NULL)
		{
			pTmp = pEdge->pNext;
			delete pEdge;
			pEdge = pTmp;
		}

		m_pTinMesh->pEdgeArr = NULL;

		return;
	}

	// Algorithm IncrementalDelaunay(V)
	// Input: 由n个点组成的二维点集V
	// Output: Delaunay三角剖分DT
	//	1.add a appropriate triangle boudingbox to contain V ( such as: we can use triangle abc, a=(0, 3M), b=(-3M,-3M), c=(3M, 0), M=Max({|x1|,|x2|,|x3|,...} U {|y1|,|y2|,|y3|,...}))
	//	2.initialize DT(a,b,c) as triangle abc
	//	3.for i <- 1 to n 
	//		do (Insert(DT(a,b,c,v1,v2,...,vi-1), vi))   
	//	4.remove the boundingbox and relative triangle which cotains any vertex of triangle abc from DT(a,b,c,v1,v2,...,vn) and return DT(v1,v2,...,vn).
	long SmtTinDelaunayInc::DoMesh()
	{
		// Add a appropriate triangle boudingbox to contain V
		AddBoundingBox();

		// Get a vertex/point vi from V and Insert(vi)
		for (int i=3; i<m_pTinMesh->vVers.size(); i++)
		{
			Insert(i);
		}

		// Remove the bounding box
		RemoveBoundingBox();

		//calculate triangles
		CalcTriangleCount();

		return SMT_ERR_NONE;
	}

	void SmtTinDelaunayInc::AddBoundingBox(void)
	{
		real max = 0;
		real max_x = 0;
		real max_y = 0;
		real t;

		for (int i=3; i<m_pTinMesh->vVers.size(); i++)
		{
			t = abs(m_pTinMesh->vVers[i].x);
			if (max_x < t)
			{
				max_x = t;
			}

			t = abs(m_pTinMesh->vVers[i].y);
			if (max_y < t)
			{
				max_y = t;
			}
		}

		max = max_x > max_y ? max_x:max_y;

		//TRIANGLE box;
		//box.v1 = VERTEX2D(0, 3*max);
		//box.v2 = VERTEX2D(-3*max, 3*max);
		//box.v3 = VERTEX2D(3*max, 0);

		Vertex3D v1 (0, 4*max, 0);
		Vertex3D v2 (-4*max, -4*max ,0);
		Vertex3D v3 (4*max, 0 ,0);

		// Assign to Vertex array
		m_pTinMesh->vVers[0] = v1;
		m_pTinMesh->vVers[1] = v2;
		m_pTinMesh->vVers[2] = v3;
		// add the Triangle boundingbox
		AddTriangleNode(NULL, 0, 1, 2);
	}

	void SmtTinDelaunayInc::RemoveBoundingBox(void)
	{
		int statify[3]={0,0,0};
		int vertex_index;
		int* pi;
		int k = 1;

		// Remove the first triangle-boundingbox
		//m_pTinMesh->pTriArr = m_pTinMesh->pTriArr->pNext;
		//m_pTinMesh->pTriArr->pPrev = NULL; // as head

		TrianglePtr pTri = m_pTinMesh->pTriArr;
		TrianglePtr pNext = NULL;
		while (pTri != NULL)
		{
			pNext = pTri->pNext;

			statify[0] = 0;
			statify[1] = 0;
			statify[2] = 0;

			pi = &(pTri->i1);
			for (int j=0, k = 1; j<3; j++, k*= 2)
			{			
				vertex_index = *pi++;

				if(vertex_index == 0 || vertex_index == 1 || vertex_index == 2) // bounding box vertex
				{
					statify[j] = k;
				}
			}

			switch(statify[0] | statify[1] | statify[2] )
			{
			case 0: // no statify
				break;
			case 1:
			case 2:
			case 4: // 1 statify, remove 1 triangle, 1 vertex
				RemoveTriangleNode(pTri);		
				break;
			case 3:
			case 5:
			case 6: // 2 statify, remove 1 triangle, 2 vertices
				RemoveTriangleNode(pTri);			
				break;
			case 7: // 3 statify, remove 1 triangle, 3 vertices
				RemoveTriangleNode(pTri);
				break;
			default:
				break;
			}

			// go to next item
			pTri = pNext;
		}
	}

	void SmtTinDelaunayInc::CalcTriangleCount(void)
	{
		m_pTinMesh->nTriangles = 0;
		TrianglePtr pTri = m_pTinMesh->pTriArr;
		while(pTri != NULL)	
		{
			pTri = pTri->pNext;
			m_pTinMesh->nTriangles ++;
		}

		m_pTinMesh->nTriangles = 0;
		pTri = m_pTinMesh->pTriArr;
		while(pTri != NULL)	
		{
			pTri = pTri->pNext;
			m_pTinMesh->nTriangles ++;
		}
	}

	// Return a positive value if the points pa, pb, and
	// pc occur in counterclockwise order; a negative
	// value if they occur in clockwise order; and zero
	// if they are collinear. The result is also a rough
	// approximation of twice the signed area of the
	// triangle defined by the three points.
	real SmtTinDelaunayInc::CounterClockWise(Vertex3D & a, Vertex3D & b, Vertex3D & c)
	{
		return ((b.x - a.x)*(c.y - b.y) - (c.x - b.x)*(b.y - a.y));
	}

	// Adjust if the point lies in the triangle abc
	real SmtTinDelaunayInc::InTriangle(Vertex3D & Ver, TrianglePtr pTri)
	{
		Vertex3D V1, V2, V3;
		V1 = m_pTinMesh->vVers[pTri->i1];
		V2 = m_pTinMesh->vVers[pTri->i2];
		V3 = m_pTinMesh->vVers[pTri->i3];


		real ccw1 = CounterClockWise(V1, V2, Ver);
		real ccw2 = CounterClockWise(V2, V3, Ver);
		real ccw3 = CounterClockWise(V3, V1, Ver);

		real r = -1;
		if (ccw1>0 && ccw2>0 && ccw3>0)
		{
			r = 1;
		}
		else if(ccw1*ccw2*ccw3 == 0 && (ccw1*ccw2 > 0 || ccw1*ccw3 > 0 || ccw2*ccw3 > 0) )
		{
			r = 0;
		}

		return r;
	}

	// Algorithm Insert(DT(a,b,c,v1,v2,...,vi-1), vi)
	// 1.find the triangle vavbvc which contains vi // FindTriangle()
	// 2.if (vi located at the interior of vavbvc)  
	// 3.    then add triangle vavbvi, vbvcvi and vcvavi into DT // UpdateDT()
	// FlipTest(DT, va, vb, vi)
	// FlipTest(DT, vb, vc, vi)
	// FlipTest(DT, vc, va, vi)
	// 4.else if (vi located at one edge (E.g. edge vavb) of vavbvc) 
	// 5.    then add triangle vavivc, vivbvc, vavdvi and vivdvb into DT (here, d is the third vertex of triangle which contains edge vavb) // UpdateDT()
	// FlipTest(DT, va, vd, vi)
	// FlipTest(DT, vc, va, vi)
	// FlipTest(DT, vd, vb, vi)
	// FlipTest(DT, vb, vc, vi)
	// 6.return DT(a,b,c,v1,v2,...,vi)
	void SmtTinDelaunayInc::Insert(int ver_index)
	{
		Vertex3D Ver = m_pTinMesh->vVers[ver_index];
		TrianglePtr pTargetTri = NULL;
		TrianglePtr pEqualTri1 = NULL;
		TrianglePtr pEqualTri2 = NULL;

		int j = 0;
		TrianglePtr pTri = m_pTinMesh->pTriArr;
		while (pTri != NULL)
		{
			real r = InTriangle(Ver, pTri);
			if(r > 0) // should be in triangle
			{
				pTargetTri = pTri;
			}
			else if (r == 0) // should be on edge
			{
				if(j == 0)
				{
					pEqualTri1 = pTri;
					j++;				
				}
				else
				{
					pEqualTri2 = pTri;
				}

			}

			pTri = pTri->pNext;
		}

		if (pEqualTri1 != NULL && pEqualTri2 != NULL)
		{
			InsertOnEdge(pEqualTri1, ver_index);
			InsertOnEdge(pEqualTri2, ver_index);
		}
		else
		{
			InsertInTriangle(pTargetTri, ver_index);
		}
	}

	void SmtTinDelaunayInc::InsertInTriangle(TrianglePtr pTargetTri, int ver_index)
	{
		int index_a, index_b, index_c;
		TrianglePtr pTri = NULL;
		TrianglePtr pNewTri = NULL;

		pTri = pTargetTri;	
		if(pTri == NULL)
		{
			return;
		}

		// Inset p into target triangle
		index_a = pTri->i1;
		index_b = pTri->i2;
		index_c = pTri->i3;

		// Insert edge pa, pb, pc
		for(int i=0; i<3; i++)
		{
			// allocate memory
			if(i == 0)
			{
				pNewTri = AddTriangleNode(pTri, index_a, index_b, ver_index);
			}
			else if(i == 1)
			{
				pNewTri = AddTriangleNode(pTri, index_b, index_c, ver_index);
			}
			else
			{
				pNewTri = AddTriangleNode(pTri, index_c, index_a, ver_index);
			}

			// go to next item
			if (pNewTri != NULL)
			{
				pTri = pNewTri;
			}
			else
			{
				pTri = pTri;
			}
		}

		// Get the three sub-triangles
		pTri = pTargetTri;	
		TrianglePtr pTestTri[3];
		for (int i=0; i< 3; i++)
		{
			pTestTri[i] = pTri->pNext;

			pTri = pTri->pNext;
		}

		// remove the Target Triangle
		RemoveTriangleNode(pTargetTri);

		for (int i=0; i< 3; i++)
		{
			// Flip test
			FlipTest(pTestTri[i]);
		}
	}

	void SmtTinDelaunayInc::InsertOnEdge(TrianglePtr pTargetTri, int ver_index)
	{
		int index_a, index_b, index_c;
		TrianglePtr pTri = NULL;
		TrianglePtr pNewTri = NULL;

		pTri = pTargetTri;	
		if(pTri == NULL)
		{
			return;
		}

		// Inset p into target triangle
		index_a = pTri->i1;
		index_b = pTri->i2;
		index_c = pTri->i3;

		// Insert edge pa, pb, pc
		for(int i=0; i<3; i++)
		{
			// allocate memory
			if(i == 0)
			{
				pNewTri = AddTriangleNode(pTri, index_a, index_b, ver_index);
			}
			else if(i == 1)
			{
				pNewTri = AddTriangleNode(pTri, index_b, index_c, ver_index);
			}
			else
			{
				pNewTri = AddTriangleNode(pTri, index_c, index_a, ver_index);
			}		

			// go to next item
			if (pNewTri != NULL)
			{
				pTri = pNewTri;
			}
			else
			{
				pTri = pTri;
			}
		}

		// Get the two sub-triangles
		pTri = pTargetTri;	
		TrianglePtr pTestTri[2];
		for ( int i=0; i< 2; i++)
		{
			pTestTri[i] = pTri->pNext;
			pTri = pTri->pNext;
		}

		// remove the Target Triangle
		RemoveTriangleNode(pTargetTri);

		for (int i=0; i< 2; i++)
		{
			// Flip test
			FlipTest(pTestTri[i]);
		}
	}

	// Precondition: the triangle satisfies CCW order
	// Algorithm FlipTest(DT(a,b,c,v1,v2,...,vi), va, vb, vi)
	// 1.find the third vertex (vd) of triangle which contains edge vavb // FindThirdVertex()
	// 2.if(vi is in circumcircle of abd)  // InCircle()
	// 3.    then remove edge vavb, add new edge vivd into DT // UpdateDT()
	//		  FlipTest(DT, va, vd, vi)
	//		  FlipTest(DT, vd, vb, vi)

	bool SmtTinDelaunayInc::FlipTest(TrianglePtr pTestTri)
	{
		bool flipped = false;

		int index_a = pTestTri->i1;
		int index_b = pTestTri->i2;
		int index_p = pTestTri->i3;

		int statify[3]={0,0,0};
		int vertex_index;
		int* pi;
		int k = 1;

		// find the triangle which has edge consists of start and end
		TrianglePtr pTri = m_pTinMesh->pTriArr;

		int index_d = -1;
		while (pTri != NULL)
		{
			statify[0] = 0;
			statify[1] = 0;
			statify[2] = 0;

			pi = &(pTri->i1);
			for (int j=0, k = 1; j<3; j++, k*= 2)
			{
				vertex_index = *pi++;
				if(vertex_index == index_a || vertex_index == index_b)
				{
					statify[j] = k;
				}
			}

			switch(statify[0] | statify[1] | statify[2] )
			{
			case 3:
				if(CounterClockWise(m_pTinMesh->vVers[index_a], m_pTinMesh->vVers[index_b],m_pTinMesh->vVers[pTri->i3]) < 0)
				{
					index_d = pTri->i3;
				}

				break;
			case 5:
				if(CounterClockWise(m_pTinMesh->vVers[index_a], m_pTinMesh->vVers[index_b],m_pTinMesh->vVers[pTri->i2]) < 0)
				{
					index_d = pTri->i2;
				}

				break;
			case 6: 
				if(CounterClockWise(m_pTinMesh->vVers[index_a], m_pTinMesh->vVers[index_b],m_pTinMesh->vVers[pTri->i1]) < 0)
				{
					index_d = pTri->i1;
				}

				break;

			default:
				break;
			}

			if (index_d != -1)
			{
				Vertex3D a = m_pTinMesh->vVers[index_a] ;
				Vertex3D b = m_pTinMesh->vVers[index_b];
				Vertex3D d = m_pTinMesh->vVers[index_d];
				Vertex3D p = m_pTinMesh->vVers[index_p];

				if(InCircle( a, b, p, d) < 0) // not local Delaunay
				{
					flipped = true;

					// add new triangle adp,  dbp, remove abp, abd.
					// allocate memory for adp
					TrianglePtr pT1 = AddTriangleNode(pTestTri, pTestTri->i1, index_d, pTestTri->i3);				
					// allocate memory for dbp
					TrianglePtr pT2 = AddTriangleNode(pT1, index_d, pTestTri->i2, index_p);				
					// remove abp
					RemoveTriangleNode(pTestTri);
					// remove abd
					RemoveTriangleNode(pTri);

					FlipTest(pT1); // pNewTestTri satisfies CCW order
					FlipTest(pT2); // pNewTestTri2  satisfies CCW order

					break;
				}			
			}

			// go to next item	
			pTri = pTri->pNext;
		}

		return flipped;
	}

	// In circle test, use vector cross product
	real SmtTinDelaunayInc::InCircle(Vertex3D &a, Vertex3D &b, Vertex3D &p, Vertex3D  &d)
	{
		real det;
		real alift, blift, plift, bdxpdy, pdxbdy, pdxady, adxpdy, adxbdy, bdxady;

		real adx = a.x - d.x;
		real ady = a.y - d.y;

		real bdx = b.x - d.x;
		real bdy = b.y - d.y;

		real pdx = p.x - d.x;
		real pdy = p.y - d.y;

		bdxpdy = bdx * pdy;
		pdxbdy = pdx * bdy;
		alift = adx * adx + ady * ady;

		pdxady = pdx * ady;
		adxpdy = adx * pdy;
		blift = bdx * bdx + bdy * bdy;

		adxbdy = adx * bdy;
		bdxady = bdx * ady;
		plift = pdx * pdx + pdy * pdy;

		det = alift * (bdxpdy - pdxbdy)
			+ blift * (pdxady - adxpdy)
			+ plift * (adxbdy - bdxady);

		return -det;
	}

	// Remove a node from the triangle list and deallocate the memory
	void SmtTinDelaunayInc::RemoveTriangleNode(TrianglePtr pTri)
	{
		if (pTri == NULL || m_pTinMesh == NULL)
		{
			return;
		}

		// remove from the triangle list
		if (pTri->pPrev != NULL)
		{
			pTri->pPrev->pNext = pTri->pNext;
		}
		else // remove the head, need to reset the root node
		{
			m_pTinMesh->pTriArr = pTri->pNext;
		}

		if (pTri->pNext != NULL)
		{
			pTri->pNext->pPrev = pTri->pPrev;
		}

		// deallocate memory
		delete pTri;	
	}

	// Create a new node and add it into triangle list
	TrianglePtr SmtTinDelaunayInc::AddTriangleNode(TrianglePtr pPrevTri, int i1, int i2, int i3)
	{
		// test if 3 vertices are co-linear
		if(CounterClockWise(m_pTinMesh->vVers[i1], m_pTinMesh->vVers[i2], m_pTinMesh->vVers[i3]) == 0)
		{
			return NULL;
		}

		// allocate memory
		TrianglePtr pNewTestTri = new Triangle;

		pNewTestTri->i1 = i1;
		pNewTestTri->i2 = i2;
		pNewTestTri->i3 = i3;

		// insert after prev triangle
		if (pPrevTri == NULL) // add root
		{
			m_pTinMesh->pTriArr = pNewTestTri;
			pNewTestTri->pNext = NULL;
			pNewTestTri->pPrev = NULL;
		}
		else
		{
			pNewTestTri->pNext = pPrevTri->pNext;
			pNewTestTri->pPrev = pPrevTri;

			if(pPrevTri->pNext != NULL)
			{
				pPrevTri->pNext->pPrev = pNewTestTri;
			}

			pPrevTri->pNext = pNewTestTri;
		}

		return pNewTestTri;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtTinDelaunayInc::RemoveRestrictEdgeNode(EdgePtr pEdge)
	{
		if (pEdge == NULL)
		{
			return;
		}

		// remove from the triangle list
		if (pEdge->pPrev != NULL)
		{
			pEdge->pPrev->pNext = pEdge->pNext;
		}
		else // remove the head, need to reset the root node
		{
			m_pTinMesh->pEdgeArr = pEdge->pNext;
		}

		if (pEdge->pNext != NULL)
		{
			pEdge->pNext->pPrev = pEdge->pPrev;
		}

		// deallocate memory
		delete pEdge;	
	}

	EdgePtr SmtTinDelaunayInc::AddRestrictEdgeNode(EdgePtr pPrevEdge, Vertex3D verStart,Vertex3D verEnd)
	{
		EdgePtr pNewEdge = new Edge;
		pNewEdge->verStart = verStart;
		pNewEdge->verEnd   = verEnd;

		// insert after prev edge
		if (pPrevEdge == NULL) // add root
		{
			m_pTinMesh->pEdgeArr = pNewEdge;
			pNewEdge->pNext = NULL;
			pNewEdge->pPrev = NULL;
		}
		else
		{
			pNewEdge->pNext = pPrevEdge->pNext;
			pNewEdge->pPrev = pPrevEdge;

			if(pPrevEdge->pNext != NULL)
			{
				pPrevEdge->pNext->pPrev = pNewEdge;
			}

			pPrevEdge->pNext = pNewEdge;
		}

		return pNewEdge;
	}
}