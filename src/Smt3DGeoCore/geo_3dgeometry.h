/*
File:    geo_3dgeometry.h

Desc:    Smt3DGeometry...,3D¼¸ºÎ¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GEO_3D_GEOMETRY_H
#define _GEO_3D_GEOMETRY_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "ml3d_mathlib.h"

using namespace Smt_Core;
using namespace Smt_3DMath;
namespace Smt_3DGeo
{
	//typedef  Vector3 Raw3DPoint;
	typedef  dbf3DPoint Raw3DPoint;

	enum Smt3DGeometryType
	{
		GTUnknown = 0,        
		GT3DPoint,   
		GT3DMultiPoint,     
		GT3DLineString,
		GT3DLinearRing,  
		GT3DMultiLineString,
		GT3DSurface,
		GT3DVolum,
		GT3DGeometryCollection,
		GT3DNone      	
	};

	class SMT_EXPORT_CLASS  Smt3DGeometry
	{
	public:
		Smt3DGeometry();
		virtual ~Smt3DGeometry();

		// standard Geometry
		virtual int					GetDimension() const = 0;
		virtual int					GetCoordinateDimension() const;
		virtual Smt3DGeometryType	GetGeometryType() const = 0;
		virtual const char			*GetGeometryName() const = 0;

		virtual void				SetCoordinateDimension( int nDimension ); 

		virtual bool				IsEmpty() const = 0; 
		virtual bool				IsValid() const;
		virtual bool				IsSimple() const;
		virtual bool				IsRing() const;
		virtual void				Empty() = 0;
		virtual Smt3DGeometry		*Clone() const = 0;
		virtual void				GetAabb( Aabb * psAabb ) const = 0;

		// SpatialRelation
		virtual bool				Intersects(const Smt3DGeometry * ) const;
		virtual bool				Equals( const Smt3DGeometry * ) const = 0;
		virtual bool				Disjoint( const Smt3DGeometry * ) const;
		virtual bool				Touches( const Smt3DGeometry * ) const;
		virtual bool				Crosses( const Smt3DGeometry * ) const;
		virtual bool				Within( const Smt3DGeometry * ) const;
		virtual bool				Contains( const Smt3DGeometry * ) const;

		virtual Smt3DGeometry		*GetBoundary() const;
		virtual double				Distance( const Smt3DGeometry * ) const;
		virtual Smt3DGeometry		*Buffer( double dfDist, int nQuadSegs = 30 ) const;
		virtual Smt3DGeometry		*Intersection( const Smt3DGeometry *) const;
		virtual Smt3DGeometry		*Union( const Smt3DGeometry * ) const;
		virtual Smt3DGeometry		*Difference( const Smt3DGeometry * ) const;
		virtual Smt3DGeometry		*SymmetricDifference( const Smt3DGeometry * ) const;

		// backward compatibility methods. 
		bool						Intersect( Smt3DGeometry * ) const;
		bool						Equal( Smt3DGeometry * ) const;

	protected:
		int							m_nCoordDimension;
		Aabb						m_aabb;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS Smt3DGeometryCollection : public Smt3DGeometry
	{
	public:
		Smt3DGeometryCollection();
		virtual     ~Smt3DGeometryCollection();

		// Non standard (Geometry).
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometryType	GetGeometryType() const;
		virtual Smt3DGeometry		*Clone() const;
		virtual void				Empty();

		virtual bool				IsEmpty() const;

		virtual double				GetArea() const;

		// Geometry methods
		virtual int					GetDimension() const;
		virtual void				GetAabb( Aabb * psAabb ) const;

		// GeometryCollection
		int							GetNumGeometries() const;
		Smt3DGeometry				*GetGeometryRef( int );
		const Smt3DGeometry			*GetGeometryRef( int ) const;

		// SpatialRelation
		virtual bool				Equals( const Smt3DGeometry * ) const;

		// Non standard
		virtual void				SetCoordinateDimension( int nDimension ); 
		virtual int					AddGeometry( const Smt3DGeometry * );
		virtual int					AddGeometrys( const Smt3DGeometry * ,int);
		virtual int					AddGeometryDirectly( Smt3DGeometry * );
		virtual int					AddGeometrysDirectly(Smt3DGeometry ** ,int );

		virtual int					RemoveGeometry( int iIndex, bool bDelete = true );

	private:
		int							m_nGeomCount;
		Smt3DGeometry				**m_pGeoms;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS Smt3DPoint:public Smt3DGeometry
	{
	public:
		Smt3DPoint();
		Smt3DPoint( double x, double y,double z );
		Smt3DPoint(Raw3DPoint &pt3d);
		virtual ~Smt3DPoint();

		// Geometry
		virtual void				SetCoordinateDimension( int nDimension ); 
		virtual int					GetDimension() const;
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometryType	GetGeometryType() const;


		virtual Smt3DGeometry		*Clone() const;
		virtual void				Empty();
		virtual void				GetAabb( Aabb * psAabb ) const;
		virtual bool				IsEmpty() const;

		// Point
		inline double				GetX() const { return point3D.x; } 
		inline double				GetY() const { return point3D.y; }
		inline double				GetZ() const { return point3D.z; }

		// Non standard
		inline void					SetX( double xIn ) { point3D.x = xIn; if (m_nCoordDimension == 0) m_nCoordDimension = 3; }
		inline void					SetY( double yIn ) { point3D.y = yIn; if (m_nCoordDimension == 0) m_nCoordDimension = 3; }
		inline void					SetZ( double zIn ) { point3D.z = zIn; if (m_nCoordDimension == 0) m_nCoordDimension = 3; }

		// SpatialRelation
		virtual bool				Equals( const Smt3DGeometry * ) const;

	private:
		Raw3DPoint					point3D;
	};


	class SMT_EXPORT_CLASS Smt3DMultiPoint : public Smt3DGeometryCollection
	{
	public:
		Smt3DMultiPoint();
		virtual ~Smt3DMultiPoint();

		// Non standard (Geometry).
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometryType	GetGeometryType() const;
		virtual Smt3DGeometry		*Clone() const;

		// Non standard
		virtual int					AddGeometryDirectly( Smt3DGeometry * );
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS Smt3DCurve : public Smt3DGeometry
	{
	public:
		Smt3DCurve();
		virtual ~Smt3DCurve();

		// Curve methods

		virtual double				GetLength() const = 0;
		virtual void				StartPoint(Smt3DPoint *) const = 0;
		virtual void				EndPoint(Smt3DPoint *) const = 0;
		virtual bool				IsClosed() const;
		virtual void				Value( double, Smt3DPoint * ) const = 0;
	};

	class SMT_EXPORT_CLASS Smt3DLineString : public Smt3DCurve
	{
	public:
		Smt3DLineString();
		virtual ~Smt3DLineString();

		// Geometry interface
		virtual int					GetDimension() const;
		virtual Smt3DGeometryType	GetGeometryType() const;
		virtual const char			*GetGeometryName() const;


		virtual Smt3DGeometry		*Clone() const;
		virtual void				Empty();
		virtual void				GetAabb( Aabb * psAabb ) const;
		virtual bool				IsEmpty() const;

		// Curve methods
		virtual double				GetLength() const;
		virtual void				StartPoint(Smt3DPoint *) const;
		virtual void				EndPoint(Smt3DPoint *) const;
		virtual void				Value( double, Smt3DPoint * ) const;

		// LineString methods
		int							GetNumPoints() const { return m_nPointCount; }
		void						GetPoint( int, Smt3DPoint * ) const;
		double						GetX( int i ) const { return m_pPoints[i].x; }
		double						GetY( int i ) const { return m_pPoints[i].y; }
		double						GetZ( int i ) const { return m_pPoints[i].z; }

		// SpatialRelation
		virtual bool				Equals( const Smt3DGeometry * ) const;

		// non standard.
		virtual void				SetCoordinateDimension( int nDimension ); 
		void						SetNumPoints( int );
		void						SetPoint( int, Smt3DPoint * );
		void						SetPoint( int, double, double ,double );
		void						SetPoints( int, Raw3DPoint *);
		void						SetPoints( int, double * padfX, double * padfY,double * padfZ);
		void						AddPoint( Smt3DPoint * );
		void						AddPoint( double, double ,double);

		void						GetPoints( Raw3DPoint *) const;

		void						AddSubLineString( const Smt3DLineString *, int nStartVertex = 0, int nEndVertex = -1 );

	protected:
		int							m_nPointCount;
		Raw3DPoint					*m_pPoints;
	};

	class SMT_EXPORT_CLASS Smt3DLinearRing : public Smt3DLineString
	{

	public:
		Smt3DLinearRing();
		Smt3DLinearRing( Smt3DLinearRing * );
		virtual~Smt3DLinearRing();

		// Non standard.
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometry		*Clone() const;
		virtual bool				IsClockwise() const;
		virtual void				ReverseWindingOrder();
		virtual void				CloseRings();
		bool						IsPointInRing(const Smt3DPoint* pt, bool bTestEnvelope = true) const;
		bool						IsPointOnRingBoundary(const Smt3DPoint* pt, bool bTestEnvelope = true) const;
	};

	class SMT_EXPORT_CLASS Smt3DMultiLineString : public Smt3DGeometryCollection
	{
	public:
		Smt3DMultiLineString();
		virtual ~Smt3DMultiLineString();

		// Non standard (Geometry).
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometryType	GetGeometryType() const;
		virtual Smt3DGeometry		*Clone() const;

		// Non standard
		virtual int					AddGeometryDirectly( Smt3DGeometry * );
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS Smt3DSurface : public Smt3DGeometry
	{
	public:
		Smt3DSurface(void);
		virtual ~Smt3DSurface(void);

	public:
		virtual double				GetArea() const;
		virtual int					Centroid( Smt3DPoint * poPoint ) const ;
		virtual int					PointOnSurface( Smt3DPoint * poPoint ) const;

		// Non standard (Geometry).
		virtual const char			*GetGeometryName() const;
		virtual Smt3DGeometryType	GetGeometryType() const;

		virtual Smt3DGeometry		*Clone() const;
		virtual void				Empty();
		virtual bool				IsEmpty() const;

		// Geometry
		virtual int					GetDimension() const;
		virtual void				GetAabb( Aabb * psAabb ) const;

		// SpatialRelation
		virtual bool				Equals( const Smt3DGeometry * ) const;

		// Non standard
		virtual void				SetCoordinateDimension( int nDimension ); 

		//surf 
		Smt3DPoint					GetPoint(int) const;
		int							GetPointCount() const{return m_nPointCount;}

		Smt3DTriangle				GetTriangle(int) const;
		int							GetTriangleCount() const{return m_nTraingleCount;}

		int							AddPoint( Smt3DPoint * );
		int							AddPointCollection( Smt3DPoint*,int);
		int							AddPointCollection( dbf3DPoint*,int);
		int							AddTriangle( Smt3DTriangle * );
		int							AddTriangleCollection( Smt3DTriangle *,int );
		
		int							RemoveTriangle(int iIndex);

	protected:
		int							m_nPointCount;
		Raw3DPoint					*m_pPoints;

		int							m_nTraingleCount;
		Smt3DTriangle				*m_pTriangles;
	};
}

#if     !defined(Export_Smt3DGeoCore)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DGeoCoreD.lib")
#       else
#          pragma comment(lib,"Smt3DGeoCore.lib")
#	    endif
#endif

#endif _GEO_3D_GEOMETRY_H