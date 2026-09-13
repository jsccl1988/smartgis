// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/aabb.h"
#include "render/math/obb.h"
#include "render/math/plane.h"
#include "render/math/ray.h"
#include "render/math/matrix.h"
#include "base/core/api.h"
#include <cmath>
using namespace base;

namespace render
{
	Aabb::Aabb() {
	  vcMin.set(SMT_C_INVALID_DBF_VALUE, SMT_C_INVALID_DBF_VALUE,
	            SMT_C_INVALID_DBF_VALUE);
	  vcMax = vcCenter = vcMin;
	}

	Aabb::Aabb(Vector4 _vcMin, Vector4 _vcMax) 
	{
	   vcMin = _vcMin;
	   vcMax = _vcMax;
	   vcCenter = (vcMax + vcMin) / 2.0f;
	} // constructor
  
	bool  Aabb::is_init() const
	{ 
		return !is_equal(vcMin.x,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !is_equal(vcMin.y ,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !is_equal(vcMin.z, SMT_C_INVALID_DBF_VALUE,dEPSILON) || 
			   !is_equal(vcMax.x,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !is_equal(vcMax.y ,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !is_equal(vcMax.z, SMT_C_INVALID_DBF_VALUE,dEPSILON) ;
	}

	void Aabb::merge(const Aabb&sOther)
	{
		if( is_init() && sOther.is_init() )
		{
			vcMin.x = min(vcMin.x,sOther.vcMin.x);
			vcMin.y = min(vcMin.y,sOther.vcMin.y);
			vcMin.z = min(vcMin.z,sOther.vcMin.z);

			vcMax.x = max(vcMax.x,sOther.vcMax.x);
			vcMax.y = max(vcMax.y,sOther.vcMax.y);
			vcMax.z = max(vcMax.z,sOther.vcMax.z);	
		}
		else
		{
			vcMin =  sOther.vcMin;
			vcMax =  sOther.vcMax;
		}

		vcCenter = (vcMax + vcMin) / 2.0f;
	}

	void Aabb::merge( double dfX, double dfY ,double dfZ)
	{
		if(is_init())
		{
			vcMin.x = min(vcMin.x,dfX);
			vcMin.y = min(vcMin.y,dfY);
			vcMin.z = min(vcMin.z,dfZ);

			vcMax.x = max(vcMax.x,dfX);
			vcMax.y = max(vcMax.y,dfY);
			vcMax.z = max(vcMax.z,dfZ);

			vcCenter = (vcMax + vcMin) / 2.0f;
		}
		else
		{
			vcMin.set(dfX,dfY,dfZ);
			vcCenter = vcMax = vcMin;
		}
	}

	void Aabb::merge(const Vector4& vVer)
	{
		merge(vVer.x,vVer.y,vVer.z);
	}

	void Aabb::intersect( Aabb const& sOther )
	{
		if(intersects(sOther))
		{
			if( is_init() )
			{
				vcMin.x = max(vcMin.x,sOther.vcMin.x);
				vcMax.x = min(vcMax.x,sOther.vcMax.x);

				vcMin.y = max(vcMin.y,sOther.vcMin.y);
				vcMax.y = min(vcMax.y,sOther.vcMax.y);

				vcMin.z = max(vcMin.z,sOther.vcMin.z);
				vcMax.z = min(vcMax.z,sOther.vcMax.z);
			}
			else
			{
				vcMin =  sOther.vcMin;
				vcMax =  sOther.vcMax;
			}
		}
		else
		{
			vcMin.set(0,0,0);
			vcCenter = vcMax = vcMin;
		}
	}

	bool Aabb::intersects(Aabb const& other) const
	{
		return vcMin.x <= other.vcMax.x && vcMax.x >= other.vcMin.x && 
			   vcMin.y <= other.vcMax.y && vcMax.y >= other.vcMin.y &&
			   vcMin.z <= other.vcMax.z && vcMax.z >= other.vcMin.z;
	}

	bool Aabb::contains(Aabb const& other) const
	{
		return vcMin.x <= other.vcMin.x && vcMax.x >= other.vcMax.x &&
			   vcMin.y <= other.vcMin.y && vcMax.y >= other.vcMax.y &&
			   vcMin.z <= other.vcMin.z && vcMax.z >= other.vcMax.z; 
	}

	bool Aabb::contains(Vector3 const& other) const
	{
		return vcMin.x <= other.x && vcMax.x >= other.x &&
			   vcMin.y <= other.y && vcMax.y >= other.y &&
			   vcMin.z <= other.z && vcMax.z >= other.z; 
	}


	// construct from obb
	void Aabb::construct(const Obb *pObb) 
	{
	   Vector4 vcA0, vcA1, vcA2;
	   Vector4 _vcMax, _vcMin;

	   vcA0 = pObb->vcA0 * pObb->fA0;
	   vcA1 = pObb->vcA1 * pObb->fA1;
	   vcA2 = pObb->vcA2 * pObb->fA2;

	   // find x extensions
	   if (vcA0.x > vcA1.x) 
	   {
		  if (vcA0.x > vcA2.x) 
		  {
			 vcMax.x =  vcA0.x;
			 vcMin.x = -vcA0.x;
		  }
		  else 
		  {
			 vcMax.x =  vcA2.x;
			 vcMin.x = -vcA2.x;
		  }
	   }
	   else 
	   {
		  if (vcA1.x > vcA2.x) 
		  {
			 vcMax.x =  vcA1.x;
			 vcMin.x = -vcA1.x;
		  }
		  else 
		  {
			 vcMax.x =  vcA2.x;
			 vcMin.x = -vcA2.x;
		  }
	   }
   
	   // find y extensions
	   if (vcA0.y > vcA1.y) 
	   {
		  if (vcA0.y > vcA2.y) 
		  {
			 vcMax.y =  vcA0.y;
			 vcMin.y = -vcA0.y;
		  }
		  else 
		  {
			 vcMax.y =  vcA2.y;
			 vcMin.y = -vcA2.y;
		  }
	   }
	   else 
	   {
		  if (vcA1.y > vcA2.y) 
		  {
			 vcMax.y =  vcA1.y;
			 vcMin.y = -vcA1.y;
		  }
		  else 
		  {
			 vcMax.y =  vcA2.y;
			 vcMin.y = -vcA2.y;
		  }    
	   }

	   // find z extensions
	   if (vcA0.z > vcA1.z) 
	   {
		  if (vcA0.z > vcA2.z) 
		  {
			 vcMax.z =  vcA0.z;
			 vcMin.z = -vcA0.z;
		  }
		  else 
		  {
			 vcMax.z =  vcA2.z;
			 vcMin.z = -vcA2.z;
		  }
	   }
	   else 
	   {
		  if (vcA1.z > vcA2.z) 
		  {
			 vcMax.z =  vcA1.z;
			 vcMin.z = -vcA1.z;
		  }
		  else 
		  {
			 vcMax.z =  vcA2.z;
			 vcMin.z = -vcA2.z;
			 }
		  }
	   vcMax = vcMax + pObb->vcCenter;
	   vcMin = vcMin + pObb->vcCenter;
	 } // construct
 
	/**
	 * Culls AABB to n sided frustrum. Normals pointing outwards.
	 * -> IN:  Plane   - array of planes building frustrum
	 *         int        - number of planes in array
	 *    OUT: ZFXVISIBLE - obb totally inside frustrum
	 *         ZFXCLIPPED - obb clipped by frustrum
	 *         ZFXCULLED  - obb totally outside frustrum
	 */
	CullResult Aabb::cull(const Plane *pPlanes, int nNumPlanes) 
	{
	   Vector4  vcMin, vcMax;
	   bool       bIntersects = false;

	   // find and test extreme points
	   for (int i=0; i<nNumPlanes; i++) 
	   {
		  // x coordinate
		  if (pPlanes[i].m_vcN.x >= 0.0f) 
		  {
			 vcMin.x = this->vcMin.x;
			 vcMax.x = this->vcMax.x;
		  }
		  else 
		  {
			 vcMin.x = this->vcMax.x;
			 vcMax.x = this->vcMin.x;
		  }
		  // y coordinate
		  if (pPlanes[i].m_vcN.y >= 0.0f) 
		  {
			 vcMin.y = this->vcMin.y;
			 vcMax.y = this->vcMax.y;
		  }
		  else 
		  {
			 vcMin.y = this->vcMax.y;
			 vcMax.y = this->vcMin.y;
		  }
		  // z coordinate
		  if (pPlanes[i].m_vcN.z >= 0.0f) 
		  {
			 vcMin.z = this->vcMin.z;
			 vcMax.z = this->vcMax.z;
		  }
		  else 
		  {
			 vcMin.z = this->vcMax.z;
			 vcMax.z = this->vcMin.z;
		  }

		  if ( (dot(pPlanes[i].m_vcN, vcMin) + pPlanes[i].m_fD) > 0.0f)
			 return CullResult::kCulled;

		  if ( (dot(pPlanes[i].m_vcN, vcMax) + pPlanes[i].m_fD) >= 0.0f)
			 bIntersects = true;
	   } // loop end 

	   if (bIntersects) return CullResult::kClipped;
	   return CullResult::kVisible;
	} // cull
 

	// does aabb contain the given point
	bool Aabb::intersects(const Vector4 &vc) 
	{
	   if ( vc.x > vcMax.x ) return false;
	   if ( vc.y > vcMax.y ) return false;
	   if ( vc.z > vcMax.z ) return false;
	   if ( vc.x < vcMin.x ) return false;
	   if ( vc.y < vcMin.y ) return false;
	   if ( vc.z < vcMin.z ) return false;
	   return true;
	} // intersects(point)
 

	// does aabb contain ray
	bool Aabb::contains(const Ray &ray, float fL) 
	{
	   Vector4 vcEnd = ray.m_vcOrig + (ray.m_vcDir*fL);
	   return ( intersects(ray.m_vcOrig) &&intersects(vcEnd) );

	} // contains
 

	// get the six planes, normals pointing outwards
	void Aabb::get_planes(Plane *pPlanes) 
	{
	   Vector4 vcN;
   
	   if (!pPlanes) return;

	   // right side
	   vcN.set(1.0f, 0.0f, 0.0f);
	   pPlanes[0].set(vcN, vcMax);
   
	   // left side
	   vcN.set(-1.0f, 0.0f, 0.0f);
	   pPlanes[1].set(vcN, vcMin);

	   // front side
	   vcN.set(0.0f, 0.0f, -1.0f);
	   pPlanes[2].set(vcN, vcMin);

	   // back side
	   vcN.set(0.0f, 0.0f, 1.0f);
	   pPlanes[3].set(vcN, vcMax);

	   // top side
	   vcN.set(0.0f, 1.0f, 0.0f);
	   pPlanes[4].set(vcN, vcMax);

	   // bottom side
	   vcN.set(0.0f, -1.0f, 0.0f);
	   pPlanes[5].set(vcN, vcMin);
	} // intersects(point)

}

