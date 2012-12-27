#ifndef _GEO_GEOMETRY_H
#define _GEO_GEOMETRY_H

#include "smt_bas_struct.h"
#include "bl_envelope.h"
#include "smt_matrix2d.h"

namespace Smt_Geo
{
	typedef Smt_Core::dbfPoint RawPoint;

	enum SmtSpatialRs
	{
		SS_Unkown =		0,
		SS_Within =		1,
		SS_Touches =	1<<1,
		SS_Crosses =	1<<2,
		SS_Overlaps =	1<<3,
		SS_Intersects = 1<<4,
		SS_Equals =		1<<5,
		SS_Contains =	1<<6,
		SS_Disjoint =	1<<7
	};

	enum SmtGeometryType
	{
		GTUnknown				= 0,        
		GTPoint					= 1,           
		GTLineString			= 2,
		GTSpline				= 3,
		GTArc					= 4,
		GTLinearRing			= 5,    
		GTPolygon				= 6,  
		GTFan					= 7,
		GTMultiPoint			= 8,     
		GTMultiLineString		= 9, 
		GTMultiPolygon			= 10,   
		GTGeometryCollection	= 11,
		GTGrid					= 12,
		GTTin					= 13,
		GTNone					= 14      	
	};

	enum SmtSplineType
	{
		CT_CIRCLE,
		CT_LAGSPLINE,
		CT_BZERSPLINE,
		CT_BSPLINE,
		CT_3SPLINE,
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS  SmtGeometry
	{
	public:
		SmtGeometry();
		virtual ~SmtGeometry();

		// standard Geometry
		virtual int              GetDimension() const = 0;
		virtual int              GetCoordinateDimension() const;
		virtual SmtGeometryType  GetGeometryType() const = 0;
		virtual const char		*GetGeometryName() const = 0;

		virtual void             SetCoordinateDimension( int nDimension ); 

		virtual bool             IsEmpty() const = 0; 
		virtual bool             IsValid() const;
		virtual bool             IsSimple() const;
		virtual bool             IsRing() const;
		virtual void             Empty() = 0;
		virtual SmtGeometry		*Clone() const = 0;
		virtual void             GetEnvelope(Smt_Base:: Envelope * psEnvelope ) const = 0;

		// SpatialRelation
		virtual bool             Intersects( const SmtGeometry * ) const;
		virtual bool             Equals( const SmtGeometry * ) const = 0;
		virtual bool             Disjoint( const SmtGeometry * ) const;
		virtual bool             Touches( const SmtGeometry * ) const;
		virtual bool             Crosses( const SmtGeometry * ) const;
		virtual bool             Within( const SmtGeometry * ) const;
		virtual bool             Contains( const SmtGeometry * ) const;
		virtual bool             Overlaps( const SmtGeometry * ) const;

		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;

		virtual SmtGeometry		*GetBoundary() const;
		virtual double           Distance( const SmtGeometry * ) const;
		virtual SmtGeometry		*ConvexHull() const;
		virtual SmtGeometry		*Buffer( double dfDist, int nQuadSegs = 30 ) const;
		virtual SmtGeometry		*Intersection( const SmtGeometry *) const;
		virtual SmtGeometry		*Union( const SmtGeometry * ) const;
		virtual SmtGeometry		*Difference( const SmtGeometry * ) const;
		virtual SmtGeometry		*SymmetricDifference( const SmtGeometry * ) const;

		// backward compatibility methods. 
		bool                     Intersect( SmtGeometry * ) const;
		bool                     Equal( SmtGeometry * ) const;

	protected:
		int                      m_nCoordDimension;
	};

	class SMT_EXPORT_CLASS SmtPoint:public SmtGeometry
	{
	public:
		SmtPoint();
		SmtPoint( double x, double y );
		virtual ~SmtPoint();

		// Geometry
		virtual void             SetCoordinateDimension( int nDimension ); 
		virtual int              GetDimension() const;
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;


		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual void             GetEnvelope( Smt_Base::Envelope * psEnvelope ) const;
		virtual bool             IsEmpty() const;

		// Point
		inline double            GetX() const { return x; } 
		inline double            GetY() const { return y; }

		// Non standard
		inline void              SetX( double xIn ) { x = xIn; if (m_nCoordDimension == 0) m_nCoordDimension = 2; }
		inline void              SetY( double yIn ) { y = yIn; if (m_nCoordDimension == 0) m_nCoordDimension = 2; }
		
		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;
		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;

		//
		virtual double           Distance( const SmtGeometry * ) const;

	private:
		double                   x,y;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtCurve : public SmtGeometry
	{
	public:
		SmtCurve();
		virtual ~SmtCurve();

		// Curve methods
		virtual double           GetLength() const = 0;
		virtual void             StartPoint(SmtPoint *) const = 0;
		virtual void             EndPoint(SmtPoint *) const = 0;
		virtual bool             IsClosed() const;
		virtual void             Value( double, SmtPoint * ) const = 0;
	};

	class SMT_EXPORT_DLL SmtArc : public SmtCurve
	{
	public:
		SmtArc();
		virtual ~SmtArc();

		// Geometry interface
		virtual int              GetDimension() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual const char		*GetGeometryName() const;


		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;
		virtual bool             IsEmpty() const;

		// Curve methods
		virtual double           GetLength() const;
		virtual void             StartPoint(SmtPoint *) const;
		virtual void             EndPoint(SmtPoint *) const;
		virtual void             Value( double, SmtPoint * ) const;

		// LineString methods
		float                    GetRadius( void ) const { return m_fRadius;}
		void                     GetCenterPoint(SmtPoint * oPoint)  const { oPoint->SetX(m_oPointCenter.x); oPoint->SetY(m_oPointCenter.y);}

		void                     SetRadius(float radius) {m_fRadius = radius;}
		void                     SetCenterPoint( RawPoint oPoint)  { m_oPointCenter.x  = oPoint.x;m_oPointCenter.y = oPoint.y;}
		void                     SetStartPoint( RawPoint oPoint)  { m_oPointStart.x  = oPoint.x;m_oPointStart.y = oPoint.y;}
		void                     SetEndPoint( RawPoint oPoint)  { m_oPointEnd.x  = oPoint.x;m_oPointEnd.y  = oPoint.y ;}

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

		// non standard.
		virtual void             SetCoordinateDimension( int nDimension ); 
	
	protected:
		float                    m_fRadius;
		RawPoint                 m_oPointCenter;
		RawPoint                 m_oPointStart;
		RawPoint                 m_oPointEnd;
	};

	class SMT_EXPORT_CLASS SmtLineString : public SmtCurve
	{
	public:
		SmtLineString();
		SmtLineString(SmtLineString *pLS);
		virtual ~SmtLineString();

		// Geometry interface
		virtual int              GetDimension() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual const char		*GetGeometryName() const;

		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;
		virtual bool             IsEmpty() const;

		// Curve methods
		virtual double           GetLength() const;
		virtual void             StartPoint(SmtPoint *) const;
		virtual void             EndPoint(SmtPoint *) const;
		virtual void             Value( double, SmtPoint * ) const;

		// LineString methods
		int                      GetNumPoints() const { return m_nPointCount; }
		void                     GetPoint( int, SmtPoint * ) const;
		void                     GetPoint( int, RawPoint * ) const;

		double                   GetX( int i ) const { return m_pPoints[i].x; }
		double                   GetY( int i ) const { return m_pPoints[i].y; }

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;

		//
		virtual double           Distance( const SmtGeometry * ) const;

		// non standard.
		virtual void             SetCoordinateDimension( int nDimension ); 
		void                     SetNumPoints( int );
		void                     SetPoint( int, SmtPoint * );
		void                     SetPoint( int, double, double );
		void                     SetPoints( int, RawPoint *);
		void                     SetPoints( int, double * padfX, double * padfY);
		void                     AddPoint( SmtPoint * );
		void                     AddPoint( double, double );

		void                     GetPoints( RawPoint *) const;

		void                     AddSubLineString( const SmtLineString *, int nStartVertex = 0, int nEndVertex = -1 );

	protected:
		int                      m_nPointCount;
		RawPoint				*m_pPoints;
	};

	class SMT_EXPORT_CLASS SmtSpline : public SmtLineString
	{
	public:
		SmtSpline();
		SmtSpline( SmtSpline * );
		virtual ~SmtSpline();

		// Non standard.
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry		*Clone() const;

		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;

		virtual void             Value( double, SmtPoint * ) const;

		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;

		virtual double           Distance( const SmtGeometry * ) const;

		inline long				GetAnalyticType() const {return m_nAnalyticType;}
		inline void				SetAnalyticType(long type) {m_nAnalyticType = type;}
		void					CalcAnalyticPoints(void);

		double					GetLength() const;

		void                    GetAnalyticPoints( RawPoint *) const;
		double                  GetAnalyticX( int i ) const { return m_pAnalyticPoints[i].x; }
		double                  GetAnalyticY( int i ) const { return m_pAnalyticPoints[i].y; }
		inline long				GetAnalyticPointCount(void) const {return m_nAnalyticPointNum;}

	protected:
		int						m_nAnalyticType;
		int						m_nAnalyticPointNum;
		RawPoint				*m_pAnalyticPoints;
	};

	class SMT_EXPORT_CLASS SmtLinearRing : public SmtSpline
	{
	public:
		friend class SmtPolygon; 

		SmtLinearRing();
		SmtLinearRing( SmtLinearRing * );
		virtual~SmtLinearRing();

		// Non standard.
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry		*Clone() const;
		virtual bool             IsClockwise() const;
		virtual void             ReverseWindingOrder();
		virtual void             CloseRings();
		virtual double           GetArea() const;
		bool                     IsPointInRing(const SmtPoint* pt, bool bTestEnvelope = true) const;
		bool                     IsPointOnRingBoundary(const SmtPoint* pt, bool bTestEnvelope = true) const;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtSurface : public SmtGeometry
	{
	public:
		virtual double           GetArea() const = 0;
		virtual int              Centroid( SmtPoint * poPoint ) const = 0;
		virtual int              PointOnSurface( SmtPoint * poPoint ) const = 0;
	};

	class SMT_EXPORT_CLASS SmtFan : public SmtSurface
	{
	public:
		SmtFan();
		virtual ~SmtFan();

		// Geometry interface
		virtual int              GetDimension() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual const char		*GetGeometryName() const;


		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;
		virtual bool             IsEmpty() const;

		// Surface Interface
		virtual double           GetArea() const;
		virtual int              Centroid( SmtPoint * poPoint ) const;
		virtual int              PointOnSurface( SmtPoint * poPoint ) const;

		// SmtFan methods
		SmtArc					*GetArc( void ){return m_pArc;}
		const SmtArc			*GetArc( void ) const {return m_pArc;}
	
		void                     SetArc(SmtArc * pArc);
		void                     SetArcDirectly(SmtArc * pArc);

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

		// non standard.
		virtual void             SetCoordinateDimension( int nDimension ); 

	protected:
		 SmtArc                *m_pArc;
	};

	class SMT_EXPORT_CLASS SmtPolygon : public SmtSurface
	{
	public:
		SmtPolygon();
		virtual     ~SmtPolygon();

		// Non standard (Geometry).
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;


		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual bool             IsEmpty() const;
		
		// Surface Interface
		virtual double           GetArea() const;
		virtual int              Centroid( SmtPoint * poPoint ) const;
		virtual int              PointOnSurface( SmtPoint * poPoint ) const;

		// Geometry
		virtual int              GetDimension() const;
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;

		//
		virtual double           Distance( const SmtGeometry * ) const;

		// Non standard
		virtual void             SetCoordinateDimension( int nDimension ); 

		void                     AddRing( SmtLinearRing * );
		void                     AddRingDirectly( SmtLinearRing * );

		SmtLinearRing			*GetExteriorRing();
		const SmtLinearRing		*GetExteriorRing() const;

		int                      GetNumInteriorRings() const;
		SmtLinearRing			*GetInteriorRing( int );
		const SmtLinearRing		*GetInteriorRing( int ) const;

		bool                     IsPointOnSurface( const SmtPoint * ) const;

		virtual void             CloseRings();

	private:
		int                      m_nRingCount;
		SmtLinearRing			**m_pRings;
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtGeometryCollection : public SmtGeometry
	{
	public:
		SmtGeometryCollection();
		virtual     ~SmtGeometryCollection();

		// Non standard (Geometry).
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		
		virtual bool             IsEmpty() const;
	
		virtual double           GetArea() const;

		// Geometry methods
		virtual int              GetDimension() const;
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;

		// GeometryCollection
		int                      GetNumGeometries() const;
		SmtGeometry				*GetGeometryRef( int );
		const SmtGeometry		*GetGeometryRef( int ) const;

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

		// Non standard
		virtual void             SetCoordinateDimension( int nDimension ); 
		virtual int              AddGeometry( const SmtGeometry * );
		virtual int              AddGeometryDirectly( SmtGeometry * );
		virtual int              RemoveGeometry( int iIndex, int bDelete = TRUE );

		void                     CloseRings();

	private:
		int                      m_nGeomCount;
		SmtGeometry				**m_pGeoms;

	};

	class SMT_EXPORT_CLASS SmtMultiPolygon : public SmtGeometryCollection
	{
	public:
		SmtMultiPolygon();
		virtual ~SmtMultiPolygon();

		// Non standard (Geometry).
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry     *Clone() const;

		// Non standard
		virtual int              AddGeometryDirectly( SmtGeometry * );

		virtual double           GetArea() const;
	};

	class SMT_EXPORT_CLASS SmtMultiPoint : public SmtGeometryCollection
	{
	public:
		SmtMultiPoint();
		virtual ~SmtMultiPoint();

		// Non standard (Geometry).
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry		*Clone() const;

		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;

		// Non standard
		virtual int              AddGeometryDirectly( SmtGeometry * );

		// Spatial Relationship
		virtual long			 Relationship( const SmtGeometry *,float fMargin = 0.1) const;
	};


	class SMT_EXPORT_CLASS SmtMultiLineString : public SmtGeometryCollection
	{
	public:
		SmtMultiLineString();
		virtual ~SmtMultiLineString();

		// Non standard (Geometry).
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;
		virtual SmtGeometry		*Clone() const;

		// Non standard
		virtual int              AddGeometryDirectly( SmtGeometry * );
	};

	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtGrid:public SmtGeometry
	{
	public:
		SmtGrid();
		SmtGrid(int nRow, int nCol);
		virtual ~SmtGrid();

		// Geometry
		virtual void             SetCoordinateDimension( int nDimension ); 
		virtual int              GetDimension() const;
		virtual const char		*GetGeometryName() const;
		virtual SmtGeometryType  GetGeometryType() const;


		virtual SmtGeometry		*Clone() const;
		virtual void             Empty();
		virtual void             GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;
		virtual bool             IsEmpty() const;


		//grid
		void                     SetSize(int nRow, int nCol);
		void                     ReSize(int nRow, int nCol);

		void                     GetSize(int &nRow, int &nCol)  const;

		inline Matrix2D<RawPoint>*GetGridNodeBuf() {return m_pNodeBuf;}
		inline  const Matrix2D<RawPoint>*GetGridNodeBuf()  const {return m_pNodeBuf;}

		// SpatialRelation
		virtual bool             Equals( const SmtGeometry * ) const;

	protected:
		Matrix2D<RawPoint>		*m_pNodeBuf;              //网格格点坐标 
		int                      m_nRow;	              //网格行数
		int                      m_nCol;	              //网格列数
	};

	class SMT_EXPORT_CLASS SmtTin : public SmtGeometry
	{
	public:
		SmtTin(void);
		virtual ~SmtTin(void);

	public:
		// Non standard (Geometry).
		virtual const char			*GetGeometryName() const;
		virtual SmtGeometryType		GetGeometryType() const;

		virtual SmtGeometry			*Clone() const;
		virtual void				Empty();
		virtual bool				IsEmpty() const;

		// Geometry
		virtual int					GetDimension() const;
		virtual void				GetEnvelope(Smt_Base::Envelope * psEnvelope ) const;
		// SpatialRelation
		virtual bool				Equals( const SmtGeometry * ) const;

		// Non standard
		virtual void				SetCoordinateDimension( int nDimension ); 

		SmtPoint					GetPoint(int) const;
		int							GetPointCount() const{return m_nPointCount;}

		Smt_Core::SmtTriangle		GetTriangle(int) const;
		int							GetTriangleCount() const{return m_nTraingleCount;}

		int							AddPoint( SmtPoint * );
		int							AddPointCollection( SmtPoint*,int);
		int							AddPointCollection( Smt_Core::dbfPoint*,int);
		int							AddTriangle( Smt_Core::SmtTriangle * );
		int							AddTriangleCollection( Smt_Core::SmtTriangle *,int );

		int							RemoveTriangle(int iIndex);

	protected:
		int							m_nPointCount;
		RawPoint					*m_pPoints;

		int							m_nTraingleCount;
		Smt_Core::SmtTriangle		*m_pTriangles;
	};
}


#if !defined(Export_SmtGeoCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGeoCoreD.lib")
#       else
#          pragma comment(lib,"SmtGeoCore.lib")
#	    endif  
#endif

#endif //_GEO_GEOMETRY_H