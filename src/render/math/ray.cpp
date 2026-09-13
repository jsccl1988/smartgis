// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/ray.h"
#include "render/math/aabb.h"
#include "render/math/obb.h"
#include "render/math/plane.h"
#include "render/math/matrix.h"
#include <cmath>

namespace render
{

	// transform ray into matrix space
	void Ray::de_transform(const Matrix &_m) 
	{
	   Matrix mInv;
	   Matrix m=_m;
	   // invert translation
	   m_vcOrig.x -= m._41;   
	   m_vcOrig.y -= m._42;
	   m_vcOrig.z -= m._43;

	   // delete it from matrix
	   m._41 = m._42 = m._43 = 0.0f;

	   // invert matrix and applay to ray
	   mInv.inverse_of(m);
	   m_vcOrig = mInv.transform_point(m_vcOrig);
	   m_vcDir  = mInv.transform_vector(m_vcDir);
	}


	// test for intersection with triangle
	bool Ray::intersects(const Vector4 &vc0, const Vector4 &vc1, const Vector4 &vc2, bool bCull, float *t) const {
	   Vector4 pvec, tvec, qvec;

	   Vector4 edge1 = vc1 - vc0;
	   Vector4 edge2 = vc2 - vc0;

	   pvec = m_vcDir.cross(edge2);

	   // if close to 0 ray is parallel
	   float det = dot(edge1, pvec);
	   if ( (bCull) && (det < 0.0001f) )
		  return false;
	   else if ( (det < 0.0001f) && (det > -0.0001f) )
		  return false;

	   // distance to plane, < 0 means beyond plane
	   tvec = m_vcOrig - vc0;
	   float u = dot(tvec, pvec);
	   if (u < 0.0f || u > det)
		  return false;

	   qvec = tvec.cross(edge1);
	   float v = dot(m_vcDir, qvec);
	   if (v < 0.0f || u+v > det)
		  return false;
   
	   if (t) 
	   {
		  *t = dot(edge2, qvec);
		  float fInvDet = 1.0f / det;
		  *t *= fInvDet;
	   }

	   return true;
	} // intersects(Tri)


	// test for intersection with triangle at certain length (line segment),
	// same as above but test distance to intersection vs segment length.
	bool Ray::intersects(const Vector4 &vc0, const Vector4 &vc1, const Vector4 &vc2, bool bCull, float fL, float *t) const {
	   Vector4 pvec, tvec, qvec;

	   Vector4 edge1 = vc1 - vc0;
	   Vector4 edge2 = vc2 - vc0;

	   pvec = m_vcDir.cross(edge2);

	   // if close to 0 ray is parallel
	   float det = dot(edge1, pvec);
	   if ( (bCull) && (det < 0.0001f) )
		  return false;
	   else if ( (det < 0.0001f) && (det > -0.0001f) )
		  return false;

	   // distance to plane, < 0 means beyond plane
	   tvec = m_vcOrig - vc0;
	   float u = dot(tvec, pvec);
	   if (u < 0.0f || u > det)
		  return false;

	   qvec = tvec.cross(edge1);
	   float v = dot(m_vcDir, qvec);
	   if (v < 0.0f || u+v > det)
		  return false;
   
	   if (t) 
	   {
		  *t = dot(edge2, qvec);
		  float fInvDet = 1.0f / det;
		  *t *= fInvDet;
		  // collision but not on segment?
		  if (*t > fL) return false; 
	   }
	   else 
	   {
		  // collision but not on segment?
		  float f = dot(edge2, qvec) * (1.0f / det);
		  if (f > fL) return false;
	   }

	   return true;
	} // intersects(Tri at length)

	// test for intersection with aabb, original code by Andrew Woo, 
	// from "Geometric Tools...", Morgan Kaufmann Publ., 2002
	bool Ray::intersects(const Aabb &aabb, float *t) const {
	   bool bInside = true;
	   float t0, t1, tmp;
	   float tNear = -999999.9f;
	   float tFar  =  999999.9f;
	   float epsilon = 0.00001f;
	   Vector4 MaxT;

	   // first pair of planes
	   if (std::fabs(m_vcDir.x) < epsilon) 
	   {
		  if ( (m_vcOrig.x < aabb.vcMin.x) ||(m_vcOrig.x > aabb.vcMax.x) )
			 return false;
	   }
	   t0 = (aabb.vcMin.x - m_vcOrig.x) / m_vcDir.x;
	   t1 = (aabb.vcMax.x - m_vcOrig.x) / m_vcDir.x;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;

	   // second pair of planes
	   if (std::fabs(m_vcDir.y) < epsilon) 
	   {
		  if ( (m_vcOrig.y < aabb.vcMin.y) ||(m_vcOrig.y > aabb.vcMax.y) )
			 return false;
	   }
	   t0 = (aabb.vcMin.y - m_vcOrig.y) / m_vcDir.y;
	   t1 = (aabb.vcMax.y - m_vcOrig.y) / m_vcDir.y;
	   if (t0 > t1) { tmp=t0; t0=t1; t1=tmp; }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;

	   // third pair of planes
	   if (std::fabs(m_vcDir.z) < epsilon) 
	   {
		  if ( (m_vcOrig.z < aabb.vcMin.z) ||(m_vcOrig.z > aabb.vcMax.z) )
			 return false;
	   }
	   t0 = (aabb.vcMin.z - m_vcOrig.z) / m_vcDir.z;
	   t1 = (aabb.vcMax.z - m_vcOrig.z) / m_vcDir.z;
	   if (t0 > t1) { tmp=t0; t0=t1; t1=tmp; }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;


	   if (tNear > 0) 
	   {
		   if (t) *t = tNear; 
	   }
	   else 
	   { 
		   if (t) *t = tFar; 
	   }
	   return true;
	} // intersects(Aabb)

	// test for intersection with aabb, original code by Andrew Woo, 
	// from "Geometric Tools...", Morgan Kaufmann Publ., 2002
	bool Ray::intersects(const Aabb &aabb, float fL, float *t) const {
	   bool bInside = true;
	   float t0, t1, tmp, tFinal;
	   float tNear = -999999.9f;
	   float tFar  =  999999.9f;
	   float epsilon = 0.00001f;
	   Vector4 MaxT;

	   // first pair of planes
	   if (std::fabs(m_vcDir.x) < epsilon) 
	   {
		  if ( (m_vcOrig.x < aabb.vcMin.x) ||(m_vcOrig.x > aabb.vcMax.x) )
			 return false;
	   }
	   t0 = (aabb.vcMin.x - m_vcOrig.x) / m_vcDir.x;
	   t1 = (aabb.vcMax.x - m_vcOrig.x) / m_vcDir.x;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;

	   // second pair of planes
	   if (std::fabs(m_vcDir.y) < epsilon) 
	   {
		  if ( (m_vcOrig.y < aabb.vcMin.y) || (m_vcOrig.y > aabb.vcMax.y) )
			 return false;
	   }
	   t0 = (aabb.vcMin.y - m_vcOrig.y) / m_vcDir.y;
	   t1 = (aabb.vcMax.y - m_vcOrig.y) / m_vcDir.y;
	   if (t0 > t1) { tmp=t0; t0=t1; t1=tmp; }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;

	   // third pair of planes
	   if (std::fabs(m_vcDir.z) < epsilon) 
	   {
		  if ( (m_vcOrig.z < aabb.vcMin.z) ||(m_vcOrig.z > aabb.vcMax.z) )
			 return false;
	   }
	   t0 = (aabb.vcMin.z - m_vcOrig.z) / m_vcDir.z;
	   t1 = (aabb.vcMax.z - m_vcOrig.z) / m_vcDir.z;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) tNear = t0;
	   if (t1 < tFar)  tFar = t1;
	   if (tNear > tFar) return false;
	   if (tFar < 0) return false;


	   if (tNear > 0) tFinal = tNear;
	   else tFinal = tFar;

	   if (tFinal > fL) return false;
	   if (t) *t = tFinal;
	   return true;
	} // intersects(Aabb) at length


	// test for intersection with obb, slaps method
	bool Ray::intersects(const Obb &obb, float *t) const {
	   float e, f, t1, t2, temp;
	   float tmin = -99999.9f, 
			 tmax = +99999.9f;

	   Vector4 vcP = obb.vcCenter - m_vcOrig;

	   // 1st slap
	   e = dot(obb.vcA0, vcP);
	   f = dot(obb.vcA0, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA0) / f;
		  t2 = (e - obb.fA0) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA0) > 0.0f) || ((-e + obb.fA0) < 0.0f) )
		  return false;

	   // 2nd slap
	   e = dot(obb.vcA1, vcP);
	   f = dot(obb.vcA1, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA1) / f;
		  t2 = (e - obb.fA1) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA1) > 0.0f) || ((-e + obb.fA1) < 0.0f) )
		  return false;

	   // 3rd slap
	   e = dot(obb.vcA2, vcP);
	   f = dot(obb.vcA2, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA2) / f;
		  t2 = (e - obb.fA2) / f;

		  if (t1 > t2) 
		  { 
			  temp=t1; 
			  t1=t2; 
			  t2=temp; 
		  }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA2) > 0.0f) || ((-e + obb.fA2) < 0.0f) )
		  return false;

	   if (tmin > 0.0f) 
	   {
		  if (t) *t = tmin;
		  return true;
	   }

	   if (t) *t = tmax;

	   return true;
	} // intersects(Obb)

	// test for intersection with obb at certain length (line segment),
	// slaps method but compare result if true to length prior return.
	bool Ray::intersects(const Obb &obb, float fL, float *t) const {
	   float e, f, t1, t2, temp;
	   float tmin = -99999.9f, 
			 tmax = +99999.9f;

	   Vector4 vcP = obb.vcCenter - m_vcOrig;

	   // 1st slap
	   e = dot(obb.vcA0, vcP);
	   f = dot(obb.vcA0, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA0) / f;
		  t2 = (e - obb.fA0) / f;

		  if (t1 > t2) 
		  { 
			  temp=t1; 
			  t1=t2; 
			  t2=temp; 
		  }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA0) > 0.0f) || ((-e + obb.fA0) < 0.0f) )
		  return false;

	   // 2nd slap
	   e = dot(obb.vcA1, vcP);
	   f = dot(obb.vcA1, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA1) / f;
		  t2 = (e - obb.fA1) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA1) > 0.0f) || ((-e + obb.fA1) < 0.0f) )
		  return false;

	   // 3rd slap
	   e = dot(obb.vcA2, vcP);
	   f = dot(obb.vcA2, m_vcDir);
	   if (std::fabs(f) > 0.00001f) 
	   {

		  t1 = (e + obb.fA2) / f;
		  t2 = (e - obb.fA2) / f;

		  if (t1 > t2) 
		  { 
			  temp=t1; 
			  t1=t2; 
			  t2=temp; 
		  }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - obb.fA2) > 0.0f) || ((-e + obb.fA2) < 0.0f) )
		  return false;

	   if ( (tmin > 0.0f) && (tmin <= fL) ) 
	   {
		  if (t) *t = tmin;
		  return true;
	   }

	   // intersection on line but not on segment
	   if (tmax > fL) return false;

	   if (t) *t = tmax;

	   return true;
	} // intersects(Obb at length)


	// Intersection with Plane from origin till infinity. 
	bool Ray::intersects(const Plane &plane, bool bCull, float *t, Vector4 *vcHit) const {
	   float Vd = dot(plane.m_vcN, m_vcDir);

	   // ray parallel to plane
	   if (std::fabs(Vd) < 0.00001f)
		  return false;

	   // normal pointing away from ray dir
	   // => intersection backface if any
	   if (bCull && (Vd > 0.0f))
		  return false;

	   float Vo = -( (dot(plane.m_vcN, m_vcOrig)) + plane.m_fD);

	   float _t = Vo / Vd;

	   // intersection behind ray origin
	   if (_t < 0.0f)
		  return false;

	   if (vcHit) 
	   {
		  (*vcHit) = m_vcOrig + (m_vcDir * _t);
	   }

	   if (t)
		  (*t) = _t;

	   return true;
	} // intersects(Plane)

	// Intersection with Plane at distance fL. 
	bool Ray::intersects(const Plane &plane, bool bCull, float fL,float *t, Vector4 *vcHit) const {
	   float Vd = dot(plane.m_vcN, m_vcDir);

	   // ray parallel to plane
	   if (std::fabs(Vd) < 0.00001f)
		  return false;

	   // normal pointing away from ray dir
	   // => intersection backface if any
	   if (bCull && (Vd > 0.0f))
		  return false;

	   float Vo = -( (dot(plane.m_vcN, m_vcOrig)) + plane.m_fD);

	   float _t = Vo / Vd;

	   // intersection behind ray origin or beyond valid range
	   if ( (_t < 0.0f) || (_t > fL) )
		  return false;

	   if (vcHit) 
	   {
		  (*vcHit) = m_vcOrig + (m_vcDir * _t);
	   }

	   if (t)
		  (*t) = _t;

	   return true;
	} // intersects(Plane)

}

