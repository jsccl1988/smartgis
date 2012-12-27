#include "ml3d_mathlib.h"
#include "smt_api.h"
using namespace Smt_Core;

namespace Smt_3DMath
{
	Aabb::Aabb(Vector4 _vcMin, Vector4 _vcMax) 
	{
	   vcMin = _vcMin;
	   vcMax = _vcMax;
	   vcCenter = (vcMax + vcMin) / 2.0f;
	} // constructor
  
	bool  Aabb::IsInit() const
	{ 
		return !IsEqual(vcMin.x,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !IsEqual(vcMin.y ,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !IsEqual(vcMin.z, SMT_C_INVALID_DBF_VALUE,dEPSILON) || 
			   !IsEqual(vcMax.x,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !IsEqual(vcMax.y ,SMT_C_INVALID_DBF_VALUE,dEPSILON) || !IsEqual(vcMax.z, SMT_C_INVALID_DBF_VALUE,dEPSILON) ;
	}

	void Aabb::Merge(const Aabb&sOther)
	{
		if( IsInit() && sOther.IsInit() )
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

	void Aabb::Merge( double dfX, double dfY ,double dfZ)
	{
		if(IsInit())
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
			vcMin.Set(dfX,dfY,dfZ);
			vcCenter = vcMax = vcMin;
		}
	}

	void Aabb::Merge(const Vector4& vVer)
	{
		Merge(vVer.x,vVer.y,vVer.z);
	}

	void Aabb::Intersect( Aabb const& sOther )
	{
		if(Intersects(sOther))
		{
			if( IsInit() )
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
			vcMin.Set(0,0,0);
			vcCenter = vcMax = vcMin;
		}
	}

	bool Aabb::Intersects(Aabb const& other) const
	{
		return vcMin.x <= other.vcMax.x && vcMax.x >= other.vcMin.x && 
			   vcMin.y <= other.vcMax.y && vcMax.y >= other.vcMin.y &&
			   vcMin.z <= other.vcMax.z && vcMax.z >= other.vcMin.z;
	}

	bool Aabb::Contains(Aabb const& other) const
	{
		return vcMin.x <= other.vcMin.x && vcMax.x >= other.vcMax.x &&
			   vcMin.y <= other.vcMin.y && vcMax.y >= other.vcMax.y &&
			   vcMin.z <= other.vcMin.z && vcMax.z >= other.vcMax.z; 
	}

	bool Aabb::Contains(Vector3 const& other) const
	{
		return vcMin.x <= other.x && vcMax.x >= other.x &&
			   vcMin.y <= other.y && vcMax.y >= other.y &&
			   vcMin.z <= other.z && vcMax.z >= other.z; 
	}


	// construct from obb
	void Aabb::Construct(const Obb *pObb) 
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
	int Aabb::Cull(const Plane *pPlanes, int nNumPlanes) 
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

		  if ( ((pPlanes[i].m_vcN*vcMin) + pPlanes[i].m_fD) > 0.0f)
			 return CULLED;

		  if ( ((pPlanes[i].m_vcN*vcMax) + pPlanes[i].m_fD) >= 0.0f)
			 bIntersects = true;
	   } // loop end 

	   if (bIntersects) return CLIPPED;
	   return VISIBLE;
	} // Cull
 

	// test for intersection with aabb, original code by Andrew Woo, 
	// from "Geometric Tools...", Morgan Kaufmann Publ., 2002
	bool Aabb::Intersects(const Ray &ray, float *t) 
	{
	   bool bInside = true;
	   float t0, t1, tmp;
	   float tNear = -999999.9f;
	   float tFar  =  999999.9f;
	   float epsilon = 0.00001f;
	   Vector4 MaxT;

	   // first pair of planes
	   if (_fabs(ray.m_vcDir.x) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.x < vcMin.x) ||(ray.m_vcOrig.x > vcMax.x) )
			 return false;
	   }
	   t0 = (vcMin.x - ray.m_vcOrig.x) / ray.m_vcDir.x;
	   t1 = (vcMax.x - ray.m_vcOrig.x) / ray.m_vcDir.x;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) 
		   tNear = t0;

	   if (t1 < tFar) 
		   tFar = t1;
	   if (tNear > tFar) 
		   return false;
	   if (tFar < 0) 
		   return false;

	   // second pair of planes
	   if (_fabs(ray.m_vcDir.y) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.y < vcMin.y) || (ray.m_vcOrig.y > vcMax.y) )
			 return false;
	   }
	   t0 = (vcMin.y - ray.m_vcOrig.y) / ray.m_vcDir.y;
	   t1 = (vcMax.y - ray.m_vcOrig.y) / ray.m_vcDir.y;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) 
		   tNear = t0;

	   if (t1 < tFar)  
		   tFar = t1;

	   if (tNear > tFar) 
		   return false;

	   if (tFar < 0) 
		   return false;

	   // third pair of planes
	   if (_fabs(ray.m_vcDir.z) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.z < vcMin.z) || (ray.m_vcOrig.z > vcMax.z) )
			 return false;
	   }
	   t0 = (vcMin.z - ray.m_vcOrig.z) / ray.m_vcDir.z;
	   t1 = (vcMax.z - ray.m_vcOrig.z) / ray.m_vcDir.z;
	   if (t0 > t1) 
	   { 
		   tmp=t0; 
		   t0=t1; 
		   t1=tmp; 
	   }
	   if (t0 > tNear) 
		   tNear = t0;

	   if (t1 < tFar)  
		   tFar = t1;

	   if (tNear > tFar) 
		   return false;

	   if (tFar < 0) 
		   return false;


	   if (tNear > 0) 
	   { 
		   if (t) *t = tNear; 
	   }
	   else 
	   { 
		   if (t) *t = tFar; 
	   }
	   return true;
	} // Intersects(Ray)
 

	// test for intersection with aabb, original code by Andrew Woo, 
	// from "Geometric Tools...", Morgan Kaufmann Publ., 2002
	bool Aabb::Intersects(const Ray &ray, float fL, float *t) 
	{
	   bool bInside = true;
	   float t0, t1, tmp, tFinal;
	   float tNear = -999999.9f;
	   float tFar  =  999999.9f;
	   float epsilon = 0.00001f;
	   Vector4 MaxT;

	   // first pair of planes
	   if (_fabs(ray.m_vcDir.x) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.x < vcMin.x) || (ray.m_vcOrig.x > vcMax.x) )
			 return false;
	   }
	   t0 = (vcMin.x - ray.m_vcOrig.x) / ray.m_vcDir.x;
	   t1 = (vcMax.x - ray.m_vcOrig.x) / ray.m_vcDir.x;
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
	   if (_fabs(ray.m_vcDir.y) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.y < vcMin.y) ||(ray.m_vcOrig.y > vcMax.y) )
			 return false;
	   }
	   t0 = (vcMin.y - ray.m_vcOrig.y) / ray.m_vcDir.y;
	   t1 = (vcMax.y - ray.m_vcOrig.y) / ray.m_vcDir.y;
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

	   // third pair of planes
	   if (_fabs(ray.m_vcDir.z) < epsilon) 
	   {
		  if ( (ray.m_vcOrig.z < vcMin.z) ||(ray.m_vcOrig.z > vcMax.z) )
			 return false;
	   }
	   t0 = (vcMin.z - ray.m_vcOrig.z) / ray.m_vcDir.z;
	   t1 = (vcMax.z - ray.m_vcOrig.z) / ray.m_vcDir.z;
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
	} // Intersects(Ray) at length
 

	// intersection between two aabbs
	bool Aabb::Intersects(const Aabb &aabb) 
	{
	   if ((vcMin.x > aabb.vcMax.x) || (aabb.vcMin.x > vcMax.x))
		  return false;
	   if ((vcMin.y > aabb.vcMax.y) || (aabb.vcMin.y > vcMax.y))
		  return false;
	   if ((vcMin.z > aabb.vcMax.z) || (aabb.vcMin.z > vcMax.z))
		  return false;
	   return true;
	} // Intersects(Aabb)
 

	// does aabb contain the given point
	bool Aabb::Intersects(const Vector4 &vc) 
	{
	   if ( vc.x > vcMax.x ) return false;
	   if ( vc.y > vcMax.y ) return false;
	   if ( vc.z > vcMax.z ) return false;
	   if ( vc.x < vcMin.x ) return false;
	   if ( vc.y < vcMin.y ) return false;
	   if ( vc.z < vcMin.z ) return false;
	   return true;
	} // Intersects(point)
 

	// does aabb contain ray
	bool Aabb::Contains(const Ray &ray, float fL) 
	{
	   Vector4 vcEnd = ray.m_vcOrig + (ray.m_vcDir*fL);
	   return ( Intersects(ray.m_vcOrig) &&Intersects(vcEnd) );

	} // Contains
 

	// get the six planes, normals pointing outwards
	void Aabb::GetPlanes(Plane *pPlanes) 
	{
	   Vector4 vcN;
   
	   if (!pPlanes) return;

	   // right side
	   vcN.Set(1.0f, 0.0f, 0.0f);
	   pPlanes[0].Set(vcN, vcMax);
   
	   // left side
	   vcN.Set(-1.0f, 0.0f, 0.0f);
	   pPlanes[1].Set(vcN, vcMin);

	   // front side
	   vcN.Set(0.0f, 0.0f, -1.0f);
	   pPlanes[2].Set(vcN, vcMin);

	   // back side
	   vcN.Set(0.0f, 0.0f, 1.0f);
	   pPlanes[3].Set(vcN, vcMax);

	   // top side
	   vcN.Set(0.0f, 1.0f, 0.0f);
	   pPlanes[4].Set(vcN, vcMax);

	   // bottom side
	   vcN.Set(0.0f, -1.0f, 0.0f);
	   pPlanes[5].Set(vcN, vcMin);
	} // Intersects(point)

}

