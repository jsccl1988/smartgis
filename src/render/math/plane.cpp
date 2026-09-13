// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/mathlib_3d.h"
namespace render
{

	// Calculate distance to point. Plane normal must be normalized.
	float Plane::Distance(const Vector4 &vcPoint) 
	{
	   return ( _fabs((m_vcN*vcPoint) - m_fD) );
	}


	// Classify point to plane.
	int Plane::Classify(const Vector4 &vcPoint) 
	{
	   float f = (vcPoint * m_vcN) + m_fD;
   
	   if (f >  0.00001) return FRONT;
	   if (f < -0.00001) return BACK;
	   return  PLANAR;
	}


	// clips a ray into two segments if it intersects the plane
	bool Plane::Clip(const Ray *_pRay, float fL, Ray *pF, Ray *pB) 
	{
	   Vector4 vcHit(0.0f,0.0f,0.0f);
   
	   Ray *pRay = (Ray*)_pRay;

	   // ray intersects plane at all?
	   if ( !pRay->Intersects( *this, false, fL, NULL, &vcHit) ) 
		  return false;

	   int n = Classify( _pRay->m_vcOrig );

	   // ray comes fron planes backside
	   if ( n == BACK ) 
	   {
		  if (pB) pB->Set(pRay->m_vcOrig, pRay->m_vcDir);
		  if (pF) pF->Set(vcHit, pRay->m_vcDir);
	   }
	   // ray comes from planes front side
	   else if ( n == FRONT ) 
	   {
		  if (pF) pF->Set(pRay->m_vcOrig, pRay->m_vcDir);
		  if (pB) pB->Set(vcHit, pRay->m_vcDir);
	   }

	   return true;
	} // Clip [ray]


	// Intersection of two planes. If third parameter is given the line
	// of intersection will be calculated. (www.magic-software.com)
	bool Plane::Intersects(const Plane &plane, Ray *pIntersection) 
	{
	   Vector4 vcCross;
	   float     fSqrLength;
   
	   // if crossproduct of normals 0 than planes parallel
	   vcCross = this->m_vcN.CrossProduct(plane.m_vcN);
	   fSqrLength = vcCross.GetSqrLength();

	   if (fSqrLength < 1e-08f) 
		  return false;

	   // find line of intersection
	   if (pIntersection) 
	   {
		  float fN00 = this->m_vcN.GetSqrLength();
		  float fN01 = this->m_vcN * plane.m_vcN;
		  float fN11 = plane.m_vcN.GetSqrLength();
		  float fDet = fN00*fN11 - fN01*fN01;

		  if (_fabs(fDet) < 1e-08f) 
			 return false;

		  float fInvDet = 1.0f/fDet;
		  float fC0 = (fN11*this->m_fD - fN01*plane.m_fD) * fInvDet;
		  float fC1 = (fN00*plane.m_fD - fN01*this->m_fD) * fInvDet;

		  (*pIntersection).m_vcDir  = vcCross;
		  (*pIntersection).m_vcOrig = this->m_vcN*fC0 + plane.m_vcN*fC1;
	   }

	   return true;
	} // Intersects(Plane)


	// Intersection of a plane with a triangle. If all vertices of the
	// triangle are on the same side of the plane, no intersection occured. 
	bool Plane::Intersects(const Vector4 &vc0, const Vector4 &vc1, const Vector4 &vc2) 
	{
	   int n = this->Classify(vc0);

	   if ( (n == this->Classify(vc1)) && (n == this->Classify(vc2)) )
		  return false;
	   return true;
	} // Intersects(Tri)


	// Intersection with AABB. Search for AABB diagonal that is most
	// aligned to plane normal. Test its two vertices against plane.
	// (M�ller/Haines, "Real-Time Rendering")
	bool Plane::Intersects(const Aabb &aabb) 
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

	   if ( ((m_vcN * Vmin) + m_fD) > 0.0f)
		  return false;
   
	   if ( ((m_vcN * Vmax) + m_fD) >= 0.0f)
		  return true;
  
	   return false;
	} // Intersects(AABB)


	// Intersection with OBB. Same as obb culling to frustrum planes.
	bool Plane::Intersects(const Obb &obb) 
	{
		float fRadius = _fabs( obb.fA0 * (m_vcN * obb.vcA0) ) 
					  + _fabs( obb.fA1 * (m_vcN * obb.vcA1) ) 
					  + _fabs( obb.fA2 * (m_vcN * obb.vcA2) );

		float fDistance = this->Distance(obb.vcCenter);
		return (fDistance <= fRadius);
	} // Intersects(OBB)

}