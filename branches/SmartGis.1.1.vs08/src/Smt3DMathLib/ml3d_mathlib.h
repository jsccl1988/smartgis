/*
File:    ml3d_mathlib.h

Desc:    ÈýÎ¬ÊýÑ§¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_3DMATHLIB_H
#define _SMT_3DMATHLIB_H

#include "smt_core.h"

#include <math.h>
#include <stdio.h>
#include <memory.h>     

namespace Smt_3DMath
{
	//////////////////////////////////////////////////////////////////////////
	//const
	const double				PI						=  3.14159265;       
	const double				PI_T					=  1.5707963;         //PI/2
	const double				T_PI					=  6.2831853;         //2*PI
	const float					G						=  -32.174f;          // ft/s^2
	const float					EPSILON					= 0.00001f;

	#define						DEG2RAD(a)				(PI/180*(a))
	#define						RAD2DEG(a)				(180/PI*(a))
	//////////////////////////////////////////////////////////////////////////
	//define
	#ifndef						NULL
	#define						NULL					0
	#endif

	#define						FRONT					0
	#define						BACK					1
	#define						PLANAR					2
	#define						CLIPPED					3
	#define						CULLED					4
	#define						VISIBLE					5

	//////////////////////////////////////////////////////////////////////////
	//macros
	float						_fabs(float f);

	//////////////////////////////////////////////////////////////////////////
	//pre define
	class Matrix;
	class Obb;
	class Aabb;
	class Plane;
	class Quat;
	class Polygon;	

	//////////////////////////////////////////////////////////////////////////
	//basic 4D vector
	class Vector3;
	class SMT_EXPORT_CLASS Vector4
	{
	public:
		union
		{
			struct
			{
				float x, y, z,w;
			};
			float v[4];
		};
	public:
		//construct
		Vector4(void) ;
		Vector4(float _x,float _y,float _z);
		Vector4(float _x,float _y,float _z,float _w);
		Vector4(Vector3 &vec3) ;


		//function
		inline void				Set(float _x,float _y,float _z,float _w = 1.0f);
		inline float			GetLength(void);					// length
		inline float			GetSqrLength(void) const;			// square length
		inline void				Negate(void);						// vector mult -1
		inline void				Normalize(void);					// normalize
		inline float			AngleWith( Vector4 &v);             // angle in rad
		inline void				RotateWith(const Matrix&);			// apply rotation part
		inline void				InvRotateWith(const Matrix&);		// apply inverse rotation
		inline void				Difference(const Vector4 &v1,const Vector4 &v2);       // from v1 to v2
		inline Vector4			CrossProduct(const Vector4 &v1);	// cross product
			
		void operator			+= (const Vector4 &v);				// operator +=
		void operator			-= (const Vector4 &v);				// operator -=
		void operator			*= (float f);						// scale Vector4
		void operator			/= (float f);						// scale down
		void operator			+= (float f);						// add scalar
		void operator			-= (float f);						// subtract scalar
		float  operator			* (const Vector4 &v) const;			// dot product
		Vector4 operator		* (float f)const;					// scale Vector4
		Vector4 operator		/ (float f)const;					// scalar divide
		Vector4 operator		+ (float f)const;					// add scalar
		Vector4 operator		- (float f)const;					// scale down
		Quat   operator			* (const Quat   &q) const;			// Vector4 * quaternion
		Vector4 operator		* (const Matrix &m) const;			// Vector4-matrix product
		Vector4 operator		+ (const Vector4 &v) const;			// addition
		Vector4 operator		- (const Vector4 &v) const;			// subtraction
	};

	//////////////////////////////////////////////////////////////////////////
	//basic 3D vector
	class SMT_EXPORT_CLASS Vector3
	{
	public:
		union
		{
			struct
			{
				float x, y, z;
			};
			float v[3];
		};
	public:
		//construct
		Vector3(void) ;
		Vector3(float _x,float _y,float _z);
		Vector3(Vector4 &vec4) ;

		//function
		inline void				Set(float _x,float _y,float _z);
		inline float			GetLength(void);					// length
		inline float			GetSqrLength(void) const;			// square length
		inline void				Negate(void);						// Vector4 mult -1
		inline void				Normalize(void);					// normalize
		inline float			AngleWith( Vector3 &v);             // angle in rad
		inline void				Difference(const Vector3 &v1,const Vector3 &v2);	// from v1 to v2
		void					Rotate(const Vector3 &vAxis,float theta);			//rotate by an axis 
		inline Vector3			CrossProduct(const Vector3 &v1);	// cross product

		void operator			+= (const Vector3 &v);				// operator +=
		void operator			-= (const Vector3 &v);				// operator -=
		void operator			*= (float f);						// scale Vector4
		void operator			/= (float f);						// scale down
		void operator			+= (float f);						// add scalar
		void operator			-= (float f);						// subtract scalar
		float  operator			* (const Vector3 &v) const;			// dot product
		Vector3 operator		* (float f)const;					// scale Vector4
		Vector3 operator		/ (float f)const;					// scalar divide
		Vector3 operator		+ (float f)const;					// add scalar
		Vector3 operator		- (float f)const;					// scale down
		Quat   operator			* (const Quat   &q) const;			// vector * quaternion
		Vector3 operator		+ (const Vector3 &v) const;			// addition
		Vector3 operator		- (const Vector3 &v) const;			// subtraction
	};

	//////////////////////////////////////////////////////////////////////////
	//basic 2D vector
	class SMT_EXPORT_CLASS Vector2
	{
	public:
		union
		{
			struct
			{
				float x, y;
			};
			float v[2];
		};
	public:
		//construct
		Vector2(void) ;
		Vector2(float _x,float _y);

		//function
		inline void				Set(float _x,float _y);
		inline float			GetLength(void);					// length
		inline float			GetSqrLength(void) const;			// square length
		inline void				Negate(void);						// Vector4 mult -1
		inline void				Normalize(void);					// normalize
		inline float			AngleWith( Vector2 &v);             // angle in rad
		inline void				Difference(const Vector2 &v1,const Vector2 &v2); // from v1 to v2
		void					Rotate(const Vector2 &vAxis,float theta);		//rotate by an axis 
		inline double			CrossProduct(const Vector2 &v1); // cross product


		void operator			+= (const Vector2 &v);				// operator +=
		void operator			-= (const Vector2 &v);				// operator -=
		void operator			*= (float f);						// scale Vector4
		void operator			/= (float f);						// scale down
		void operator			+= (float f);						// add scalar
		void operator			-= (float f);						// subtract scalar
		float  operator			* (const Vector2 &v) const;			// dot product
		Vector2 operator		* (float f)const;					// scale Vector4
		Vector2 operator		/ (float f)const;					// scalar divide
		Vector2 operator		+ (float f)const;					// add scalar
		Vector2 operator		- (float f)const;					// scale down
		Vector2 operator		+ (const Vector2 &v) const;			// addition
		Vector2 operator		- (const Vector2 &v) const;			// subtraction
	};

	//////////////////////////////////////////////////////////////////////////
	//basic matrix class
	class SMT_EXPORT_CLASS Matrix 
	{
	public:
		float _11, _12, _13, _14;
		float _21, _22, _23, _24;
		float _31, _32, _33, _34;
		float _41, _42, _43, _44;
		//construct
		Matrix(void) 
		{ 
			; 
		}

		inline void				Identity(void);                       // identity matrix
		inline void				RotaX(float a);                       // x axis
		inline void				RotaY(float a);                       // y axis
		inline void				RotaZ(float a);                       // z axis
		inline void				Rota(const Vector4 &vc);              // x, y and z
		inline void				Rota(float x, float y, float z);      // x, y and z
		inline void				RotaArbi(const Vector4 &vcAxis, float a);
		inline void				ApplyInverseRota(Vector4 *pvc);

		inline void				Translate(float dx, float dy, float dz);
		inline void				Rotate(float angle,float x, float y, float z);
		inline void				Scale(float sx, float sy, float sz);

		inline void				SetTranslation(Vector4 vc, bool EraseContent=false);
		inline Vector4			GetTranslation(void);

		void					SetPerspective( float fovy, float aspect, float zNear, float zFar);
		inline void				Billboard(Vector4 vcPos, Vector4 vcDir,Vector4 vcWorldUp = Vector4(0,1,0));
		inline void				LookAt(Vector4 vcPos, Vector4 vcLookAt, Vector4 vcWorldUp = Vector4(0,1,0));

		inline void				TransposeOf(const Matrix &m);			// transpose m, save result in this
		inline void				InverseOf(const Matrix &m);				// invert m, save result in this

		Matrix operator			*(const Matrix &m)const;				// matrix multiplication
		void operator			*=(const Matrix &m) ; 
		Vector4 operator		*(const Vector4 &vc)const;				// matrix Vector4 multiplication
	};  

	//////////////////////////////////////////////////////////////////////////
	//basic Ray class
	class SMT_EXPORT_CLASS Ray
	{
	public:
		Vector4 m_vcOrig,		// ray origin
				m_vcDir;		// ray direction

		//construct
		Ray(void) 
		{  
			; 
		}

		//function to be called
		inline void				Set(Vector4 vcOrig, Vector4 vcDir);
		inline void				DeTransform(const Matrix &_m);			// move to matrixspace

		bool					Intersects(const Vector4 &vc0, const Vector4 &vc1,const Vector4 &vc2, bool bCull,float *t);
		bool					Intersects(const Vector4 &vc0, const Vector4 &vc1,const Vector4 &vc2, bool bCull,float fL, float *t);
		bool					Intersects(const Plane &plane, bool bCull,float *t, Vector4 *vcHit);       
		bool					Intersects(const Plane &plane, bool bCull,float fL, float *t, Vector4 *vcHit);
		bool					Intersects(const Aabb &aabb, float *t);
		bool					Intersects(const Aabb &aabb, float fL, float *t);
		bool					Intersects(const Obb &obb, float *t);
		bool					Intersects(const Obb &obb, float fL, float *t);
	};

	//////////////////////////////////////////////////////////////////////////
	//basic plane class
	class SMT_EXPORT_CLASS Plane 
	{
	public:
		Vector4    m_vcN,       // plane normal vector
				   m_vcPoint;   // point on plane
		float      m_fD;        // distance to origin

		//construct
		Plane(void) 
		{ 
			; 
		}

		//function to be called
		inline void				Set(const Vector4 &vcN, const Vector4 &vcP);
		inline void				Set(const Vector4 &vcN, const Vector4 &vcP, float fD);
		inline void				Set(const Vector4 &v0,  const Vector4 &v1, const Vector4 &v2);
		inline float			Distance(const Vector4 &vcPoint);
		inline int				Classify(const Vector4 &vcPoint);
		int						Classify(const Polygon &polygon);

		bool					Clip(const Ray*, float, Ray*, Ray*);

		bool					Intersects(const Vector4 &vc0, const Vector4 &vc1, const Vector4 &vc2);
		bool					Intersects(const Plane &plane, Ray *pIntersection);
		bool					Intersects(const Aabb &aabb);
		bool					Intersects(const Obb &obb);

	};  

	//////////////////////////////////////////////////////////////////////////
	//basic orientedbox class
	class SMT_EXPORT_CLASS Obb 
	{
	public:
		float   fA0,  fA1,  fA2;	// half axis length
		Vector4	vcA0, vcA1, vcA2;	// box axis
		Vector4	vcCenter;			// centerpoint

		//construct
		Obb(void) 
		{ 
			; 
		}

		//functions to be called
		inline void				DeTransform(const Obb &obb, const Matrix &m);

		bool					Intersects(const Ray &ray, float *t);
		bool					Intersects(const Ray &ray, float fL, float *t);
		bool					Intersects(const Obb &obb);
		bool					Intersects(const Vector4 &v0, const Vector4 &v1,const Vector4 &v2);

		int						Cull(const Plane *pPlanes, int nNumPlanes);      

	private:
		void					ObbProj(const Obb &obb, const Vector4 &vcV, float *pfMin, float *pfMax);
		void					TriProj(const Vector4 &v0, const Vector4 &v1, const Vector4 &v2, const Vector4 &vcV, float *pfMin, float *pfMax);
	};

	//////////////////////////////////////////////////////////////////////////
	//basic axisalignedbox class
	class SMT_EXPORT_CLASS Aabb 
	{
	public:
		Vector4 vcMin, vcMax;	// box extreme points
		Vector4 vcCenter;		// centerpoint


		//construct
		Aabb(void) 
		{ 
			vcMin.Set(SMT_C_INVALID_DBF_VALUE,SMT_C_INVALID_DBF_VALUE,SMT_C_INVALID_DBF_VALUE); 
			vcMax = vcCenter = vcMin;
		}
		Aabb(Vector4 vcMin, Vector4 vcMax);

		bool					IsInit() const;
		//merge
		void					Merge(const Aabb&aabb);
		void					Merge( double dfX, double dfY ,double dfZ);
		void					Merge(const Vector4& vVer);
		void					Intersect( Aabb const& sOther );
		bool					Intersects(Aabb const& other) const;
		bool					Contains(Aabb const& other) const;
		bool					Contains(Vector3 const& other) const;

		//function to be calles
		void					Construct(const Obb *pObb);                  // build from obb
		int						Cull(const Plane *pPlanes, int nNumPlanes);  

		// normals pointing outwards
		void					GetPlanes(Plane *pPlanes);

		bool					Contains(const Ray &Ray, float fL);
		bool					Intersects(const Ray &Ray, float *t);
		bool					Intersects(const Ray &Ray, float fL, float *t);
		bool					Intersects(const Aabb &aabb);
		bool					Intersects(const Vector4 &vc0);
	}; 

	//////////////////////////////////////////////////////////////////////////
	//basic polygon class

	class SMT_EXPORT_CLASS Polygon 
	{
		friend class Plane;        // access for easier classifying

	private:
		Plane		   m_Plane;    // plane which poly lies in

		int            m_NumP;     // number of points
		int            m_NumI;     // number of indices
		Aabb           m_Aabb;     // bounding box
		unsigned int   m_Flag;     // whatever you want it to be

		void					CalcBoundingBox(void);

	public:

		//construct
		Polygon(void);
		~Polygon(void);


		//member  data
		Vector4    *m_pPoints;  // list of points
		unsigned int *m_pIndis; // index list

		//function to be called
		void					Set(const Vector4 *pPoints, int nNumP,const unsigned int *pIndis, int nNumI);

		void					Clip(const Plane &Plane, Polygon *pFront, Polygon *pBack);
		void					Clip(const Aabb &aabb);
		int						Cull(const Aabb &aabb);

		void					CopyOf( const Polygon &Poly );

		void					SwapFaces(void);

		bool					Intersects(const Ray &Ray, bool, float *t);
		bool					Intersects(const Ray &Ray, bool, float fL, float *t);

		int						GetNumPoints(void)      { return m_NumP;    }
		int						GetNumIndis(void)       { return m_NumI;    }
		Vector4*				GetPoints(void)         { return m_pPoints; }
		unsigned int*			GetIndices(void)        { return m_pIndis;  }
		Plane					GetPlane(void)          { return m_Plane;   }
		Aabb					GetAabb(void)           { return m_Aabb;    }
		unsigned int			GetFlag(void)           { return m_Flag;    }
		void					SetFlag(unsigned int n) { m_Flag = n;       }

		// DEBUG ONLY
		void					Print(FILE*);
	};

	//////////////////////////////////////////////////////////////////////////
	//basic quaternion class
	class SMT_EXPORT_CLASS Quat 
	{
	public:
		float x, y, z, w;

		//---------------------------------------

		Quat(void) 
		{ 
			x=0.0f, y=0.0f, z=0.0f, w=1.0f; 
		}
		Quat(float _x, float _y, float _z, float _w)
		{ 
			x=_x; y=_y; z=_z; w=_w; 
		}

		void					MakeFromEuler(float fPitch, float fYaw, float fRoll);
		void					Normalize();
		void					Conjugate(Quat q);
		void					GetEulers(float *fPitch, float *fYaw, float *fRoll);
		void					GetMatrix(Matrix *m);
		float					GetMagnitude(void);


		void operator			/= (float f);
		Quat operator			/  (float f);

		void operator			*= (float f);
		Quat operator			*  (float f);

		Quat operator			*  (const Vector4 &v) const;

		Quat operator			*  (const Quat &q) const;
		void operator			*= (const Quat &q);

		void operator			+= (const Quat &q);
		Quat operator			+  (const Quat &q) const;

		Quat operator			~  (void) const 
		{ 
			return Quat(-x, -y, -z, w); 
		}

		void					Rotate(const Quat &q1, const Quat &q2);
		Vector4					Rotate(const Vector4 &v);

	}; 
}

#if     !defined(Export_Smt3DMathLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMathLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMathLib.lib")
#	    endif
#endif

#endif //_SMT_3DLIB_H