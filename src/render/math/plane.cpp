// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/plane.h"
#include "render/math/aabb.h"
#include "render/math/obb.h"
#include "render/math/ray.h"
#include <cmath>
namespace render
{

	// Calculate distance to point. Plane normal must be normalized.
	float Plane::distance(const Vector4 &vcPoint) 
	{
	   return ( std::fabs(dot(m_vcN, vcPoint) - m_fD) );
	}


	// classify point to plane.
	PlaneSide Plane::classify(const Vector4 &vcPoint) 
	{
	   float f = dot(vcPoint, m_vcN) + m_fD;
   
	   if (f >  0.00001) return PlaneSide::kFront;
	   if (f < -0.00001) return PlaneSide::kBack;
	   return  PlaneSide::kPlanar;
	}


	// clips a ray into two segments if it intersects the plane
	bool Plane::clip(const Ray *_pRay, float fL, Ray *pF, Ray *pB) 
	{
	   Vector4 vcHit(0.0f,0.0f,0.0f);
   
	   Ray *pRay = (Ray*)_pRay;

	   // ray intersects plane at all?
	   if ( !pRay->intersects( *this, false, fL, nullptr, &vcHit) ) 
		  return false;

	   PlaneSide n = classify( _pRay->m_vcOrig );

	   // ray comes fron planes backside
	   if ( n == PlaneSide::kBack ) 
	   {
		  if (pB) pB->set(pRay->m_vcOrig, pRay->m_vcDir);
		  if (pF) pF->set(vcHit, pRay->m_vcDir);
	   }
	   // ray comes from planes front side
	   else if ( n == PlaneSide::kFront ) 
	   {
		  if (pF) pF->set(pRay->m_vcOrig, pRay->m_vcDir);
		  if (pB) pB->set(vcHit, pRay->m_vcDir);
	   }

	   return true;
	} // clip [ray]


	// Intersection of two planes. If third parameter is given the line
	// of intersection will be calculated. (www.magic-software.com)
	bool Plane::intersects(const Plane &plane, Ray *pIntersection) 
	{
	   Vector4 vcCross;
	   float     fSqrLength;
   
	   // if crossproduct of normals 0 than planes parallel
	   vcCross = this->m_vcN.cross(plane.m_vcN);
	   fSqrLength = vcCross.length_squared();

	   if (fSqrLength < 1e-08f) 
		  return false;

	   // find line of intersection
	   if (pIntersection) 
	   {
		  float fN00 = this->m_vcN.length_squared();
		  float fN01 = dot(this->m_vcN, plane.m_vcN);
		  float fN11 = plane.m_vcN.length_squared();
		  float fDet = fN00*fN11 - fN01*fN01;

		  if (std::fabs(fDet) < 1e-08f) 
			 return false;

		  float fInvDet = 1.0f/fDet;
		  float fC0 = (fN11*this->m_fD - fN01*plane.m_fD) * fInvDet;
		  float fC1 = (fN00*plane.m_fD - fN01*this->m_fD) * fInvDet;

		  (*pIntersection).m_vcDir  = vcCross;
		  (*pIntersection).m_vcOrig = this->m_vcN*fC0 + plane.m_vcN*fC1;
	   }

	   return true;
	} // intersects(Plane)


	// Intersection of a plane with a triangle. If all vertices of the
	// triangle are on the same side of the plane, no intersection occured. 
	bool Plane::intersects(const Vector4 &vc0, const Vector4 &vc1, const Vector4 &vc2) 
	{
	   PlaneSide n = this->classify(vc0);

	   if ( (n == this->classify(vc1)) && (n == this->classify(vc2)) )
		  return false;
	   return true;
	} // intersects(Tri)


	// Intersection with AABB. Search for AABB diagonal that is most
	// aligned to plane normal. Test its two vertices against plane.
	// (M�ller/Haines, "Real-Time Rendering")
	bool Plane::intersects(const Aabb &aabb) 
	{
	   Vector4 Vmin, Vmax;

	   // x component
	   if (m_vcN.x >= 0.0f) 
	   {
		  Vmin.x = aabb.vcMin.x;
		  Vmax.x = aabb.vcMax.x;
	   }
	   else 
	   {
		  Vmin.x = aabb.vcMax.x;
		  Vmax.x = aabb.vcMin.x;
	   }

	   // y component
	   if (m_vcN.y >= 0.0f) 
	   {
		  Vmin.y = aabb.vcMin.y;
		  Vmax.y = aabb.vcMax.y;
	   }
	   else 
	   {
		  Vmin.y = aabb.vcMax.y;
		  Vmax.y = aabb.vcMin.y;
	   }
   
	   // z component
	   if (m_vcN.z >= 0.0f) 
	   {
		  Vmin.z = aabb.vcMin.z;
		  Vmax.z = aabb.vcMax.z;
	   }
	   else 
	   {
		  Vmin.z = aabb.vcMax.z;
		  Vmax.z = aabb.vcMin.z;
	   }

	   if ( ((dot(m_vcN, Vmin)) + m_fD) > 0.0f)
		  return false;
   
	   if ( ((dot(m_vcN, Vmax)) + m_fD) >= 0.0f)
		  return true;
  
	   return false;
	} // intersects(AABB)


	// Intersection with OBB. Same as obb culling to frustrum planes.
	bool Plane::intersects(const Obb &obb) 
	{
		float fRadius = std::fabs( obb.fA0 * (dot(m_vcN, obb.vcA0)) ) 
					  + std::fabs( obb.fA1 * (dot(m_vcN, obb.vcA1)) ) 
					  + std::fabs( obb.fA2 * (dot(m_vcN, obb.vcA2)) );

		float fDistance = this->distance(obb.vcCenter);
		return (fDistance <= fRadius);
	} // intersects(OBB)

}