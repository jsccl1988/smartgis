/*
File:   tin_delaunay_incremental.h

Desc:    SmtTinDelaunayInc,Èý½ÇÍø
		 Incremental mtd

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _TIN_DELAUNAY_INCREMENTAL_H
#define _TIN_DELAUNAY_INCREMENTAL_H

#include "tin_tinx.h"

#define dpUnSetted (-9999)

#define NotIn(a1,a,b,c) (a1 != a && a1 != b && a1 != c)

namespace Smt_TinMesh
{
	struct Vertex3D
	{
		real x;
		real y;
		real z;
		
		Vertex3D(real _x,real _y,real _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}

		Vertex3D()
		{
			x = y = z = 0.;
		}

		bool operator ==(Vertex3D &p)
		{
			return (SMT_EQUAL(p.x,x) && SMT_EQUAL(p.y,y) );
		};
	
	};

	typedef	Vertex3D*			Vertex3DPtr;
	typedef vector<Vertex3D>	vVertex3Ds;

	struct Edge
	{
		Vertex3D verStart;
		Vertex3D verEnd;

		Edge	*pNext;
		Edge	*pPrev;

		Edge()
		{
			pNext = NULL;
			pPrev = NULL;
		}
	};

	typedef Edge*		EdgePtr;

	struct Triangle
	{
		int i1;       
		int i2;          
		int i3; 

		Triangle* pNext;
		Triangle* pPrev;

		Triangle()
		{
			i1 = 0;
			i2 = 0;
			i3 = 0;

			pNext = NULL;
			pPrev = NULL;
		}	

		bool Compare(Triangle & tri)
		{
			return !(NotIn(i1,tri.i1,tri.i2,tri.i3) || 
					NotIn(i2,tri.i1,tri.i2,tri.i3) || 
					NotIn(i3,tri.i1,tri.i2,tri.i3));
		}

	};

	typedef Triangle*		TrianglePtr;
	struct TinMesh
	{
		TrianglePtr pTriArr; 
		EdgePtr		pEdgeArr;
		vVertex3Ds	vVers;    

		int			nTriangles;
	
		TinMesh()
		{
			nTriangles = 0;
			pEdgeArr = NULL;
			pTriArr = NULL;
		}
	};

	typedef TinMesh*			TinMeshPtr;

	class SMT_EXPORT_CLASS SmtTinDelaunayInc:public SmtTinX
	{
	public:
		SmtTinDelaunayInc(void);
		virtual ~SmtTinDelaunayInc(void);

	public:
		//1.
		long					SetVertexs(const Vector3 *pVector3Ds,int nCount);
		long					SetVertexs(const vector<Vector3> &vVector3Ds);

		//2.
		long					DoMesh();

		//3.
		long					CvtTo3DSurf(Smt3DSurface *p3DSurf);
		long					CvtTo2DTin(SmtTin *pTin);	

	public:
		inline TinMeshPtr		GetTinMeshPtr(void){return m_pTinMesh;}
		
		static long				TinMeshTo3DSurf(Smt3DSurface *p3DSurf,TinMeshPtr pMesh);

	protected:
		void					AddBoundingBox(void);
		void					RemoveBoundingBox(void);
		void					CalcTriangleCount(void);
		void					UnInitTinMesh();
	
		void					Insert(int ver_index);
		bool					FlipTest(TrianglePtr pTestTri);

		real					InCircle(Vertex3D & pa, Vertex3D & pb, Vertex3D & pp, Vertex3D & pd);
		real					InTriangle(Vertex3D & pVer, TrianglePtr pTri);

		void					InsertInTriangle(TrianglePtr pTargetTri, int ver_index);
		void					InsertOnEdge(TrianglePtr pTargetTri, int ver_index);

		// Helper functions
		void					RemoveTriangleNode(TrianglePtr pTri);
		TrianglePtr				AddTriangleNode(TrianglePtr pPrevTri, int i1, int i2, int i3);

		real					CounterClockWise(Vertex3D &pa, Vertex3D &pb, Vertex3D &pc);

		void					RemoveRestrictEdgeNode(EdgePtr pEdge);
		EdgePtr					AddRestrictEdgeNode(EdgePtr pEdge, Vertex3D verStart,Vertex3D verEnd);

	protected:
		TinMeshPtr				m_pTinMesh;
	};
}

#if !defined(Export_SmtTinMesh)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtTinMeshD.lib")
#       else
#          pragma comment(lib,"SmtTinMesh.lib")
#	    endif  
#endif

#endif //_TIN_DELAUNAY_INCREMENTAL_H