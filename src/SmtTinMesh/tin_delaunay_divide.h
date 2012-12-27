/*
File:   tin_delaunay_divide.h

Desc:    SmtTinDelaunayDiv,Èý½ÇÍø
		 Divide mtd

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _TIN_DELAUNAY_DIVIDE_H
#define _TIN_DELAUNAY_DIVIDE_H

#include "tin_tinx.h"

namespace Smt_TinMesh
{
#define SYSV
#define OUTPUT
#define TIME

#ifndef NULL
#define  NULL  0
#endif
#define  TRUE  1
#define  FALSE  0
#define  Org(e)    ((e)->org)
#define  Dest(e)    ((e)->dest)
#define  Onext(e)  ((e)->onext)
#define  Oprev(e)  ((e)->oprev)
#define  Dnext(e)  ((e)->dnext)
#define  Dprev(e)  ((e)->dprev)

#define  Other_point(e,p)  ((e)->org == p ? (e)->dest : (e)->org)
#define  Next(e,p)  ((e)->org == p ? (e)->onext : (e)->dnext)
#define  Prev(e,p)  ((e)->org == p ? (e)->oprev : (e)->dprev)

#define  Visited(p)  ((p)->f)

#define Identical_refs(e1,e2)  (e1 == e2)
#define Vector(p1,p2,u,v) (u = p2->x - p1->x, v = p2->y - p1->y)

#define Cross_product_2v(u1,v1,u2,v2) (u1 * v2 - v1 * u2)

#define Cross_product_3p(p1,p2,p3) ((p2->x - p1->x) * (p3->y - p1->y) - (p2->y - p1->y) * (p3->x - p1->x))

#define Dot_product_2v(u1,v1,u2,v2) (u1 * u2 + v1 * v2)
	/* Edge sides. */
	typedef enum {right, left} side;

	/* Geometric and topological entities. */
	typedef  unsigned int index;
	typedef  unsigned int cardinal;
	typedef  int integer;
	//typedef  float  real;
	typedef  float  ordinate;
	typedef  unsigned char boolean;
	typedef  struct   point   point;
	typedef  struct  edge  edge;

	struct point {
		ordinate x,y,z;
		edge *entry_pt;
	};

	struct edge {
		point *org;
		point *dest;
		edge *onext;
		edge *oprev;
		edge *dnext;
		edge *dprev;
	};

	class SMT_EXPORT_CLASS SmtTinDelaunayDiv:public SmtTinX
	{
	public:
		SmtTinDelaunayDiv();
		virtual ~SmtTinDelaunayDiv();

	public:
		//1.
		long					SetVertexs(const Vector3 *pVector3Ds,int nCount);
		long					SetVertexs(const vector<Vector3> &vVector3Ds);

		//2.
		long					DoMesh();

		//3.
		long					CvtTo3DSurf(Smt3DSurface *p3DSurf);
		long					CvtTo2DTin(SmtTin *pTin);	

	protected:
		void					alloc_memory(cardinal n);
	
	private:
		edge					*make_edge(point *u, point *v);
		void					merge_sort(point *p[], point *p_temp[], index l, index r);
		void					divide(point *p_sorted[], index l, index r, edge **l_ccw, edge **r_cw);
		void					splice(edge *a, edge *b, point *v);
		edge					*join(edge *a, point *u, edge *b, point *v, side s);
		void					merge(edge *r_cw_l, point *s, edge *l_ccw_r, point *u, edge **l_tangent);
		void					lower_tangent(edge *r_cw_l, point *s, edge *l_ccw_r, point *u,
									edge **l_lower, point **org_l_lower,
									edge **r_lower, point **org_r_lower);
		void					delete_edge(edge *e);
		void					free_edge(edge *e);
		edge					*get_edge();
		void					free_memory();
		void					print_results(cardinal n);

		//
		void					change_ccw();
		real					is_ccw(point & a, point & b, point & c);

	private:
		edge					*e_array;
		edge					**free_list_e;
		cardinal				n_free_e;
		int						n_count;

	private:
		point					*p_array;
		vector<int>				ary1;
		vector<int>				ary2;
		vector<int>				ary3;
	};
}

#if !defined(Export_SmtTinMesh)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtTinMeshD.lib")
#       else
#          pragma comment(lib,"SmtTinMesh.lib")
#	    endif  
#endif

#endif // _TIN_DELAUNAY_MERGE_H
