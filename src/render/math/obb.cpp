// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/obb.h"
#include "render/math/matrix.h"
#include "render/math/plane.h"
#include "render/math/vector.h"
#include <cmath>
namespace render
{

	void Obb::de_transform(const Obb &obb, const Matrix &m) 
	{
	   Matrix mat = m;
	   Vector4 vcT;
   
	   // erase translation from mat
	   vcT.set(mat._41, mat._42, mat._43);
	   mat._41 = mat._42 = mat._43 = 0.0f;

	   // rotate center and axis to matrix coord.-space
	   this->vcCenter = mat.transform_point(obb.vcCenter);
	   this->vcA0     = mat.transform_vector(obb.vcA0);
	   this->vcA1     = mat.transform_vector(obb.vcA1);
	   this->vcA2     = mat.transform_vector(obb.vcA2);

	   // set translation
	   this->vcCenter += vcT;

	   // copy axis length
	   fA0 = obb.fA0;
	   fA1 = obb.fA1;
	   fA2 = obb.fA2;
	} // Transform
 
	// helperfunction
	void Obb::obb_proj(const Obb &Obb, const Vector4 &vcV,float *pfMin, float *pfMax) 
	{
	   float fDP = dot(vcV, Obb.vcCenter);
	   float fR = Obb.fA0 * std::fabs(dot(vcV, Obb.vcA0)) +
				  Obb.fA0 * std::fabs(dot(vcV, Obb.vcA1)) +
				  Obb.fA1 * std::fabs(dot(vcV, Obb.vcA2));
	   *pfMin = fDP - fR;
	   *pfMax = fDP + fR;
	} // obb_proj
 

	// helperfunction
	void Obb::tri_proj(const Vector4 &v0, const Vector4 &v1, const Vector4 &v2, const Vector4 &vcV,float *pfMin, float *pfMax) 
	{
	   *pfMin = dot(vcV, v0);
	   *pfMax = *pfMin;
   
	   float fDP = dot(vcV, v1);
	   if (fDP < *pfMin) *pfMin = fDP;
	   else if (fDP > *pfMax) *pfMax = fDP;
   
	   fDP = dot(vcV, v2);
	   if (fDP < *pfMin) *pfMin = fDP;
	   else if (fDP > *pfMax) *pfMax = fDP;
	} // tri_proj
 

	// intersects trianlge
	bool Obb::intersects(const Vector4 &v0, const Vector4 &v1, const Vector4 &v2) 
	{
	   float     fMin0, fMax0, fMin1, fMax1;
	   float     fD_C;
	   Vector4 vcV, vcTriEdge[3], vcA[3];
   
	   // just for looping
	   vcA[0] = this->vcA0;
	   vcA[1] = this->vcA1;
	   vcA[2] = this->vcA2;

	   // direction of tri-normals
	   vcTriEdge[0] = v1 - v0;
	   vcTriEdge[1] = v2 - v0;

	   vcV = vcTriEdge[0].cross(vcTriEdge[1]);

	   fMin0 = dot(vcV, v0);
	   fMax0 = fMin0;

	   this->obb_proj((*this), vcV, &fMin1, &fMax1);
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;
   
	   // direction of obb planes
	   // =======================
	   // axis 1:
	   vcV = this->vcA0;
	   this->tri_proj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = dot(vcV, this->vcCenter);
	   fMin1 = fD_C - this->fA0;
	   fMax1 = fD_C + this->fA0;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;
   
	   // axis 2:
	   vcV = this->vcA1;
	   this->tri_proj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = dot(vcV, this->vcCenter);
	   fMin1 = fD_C - this->fA1;
	   fMax1 = fD_C + this->fA1;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;

	   // axis 3:
	   vcV = this->vcA2;
	   this->tri_proj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = dot(vcV, this->vcCenter);
	   fMin1 = fD_C - this->fA2;
	   fMax1 = fD_C + this->fA2;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;


	   // direction of tri-obb edge-crossproducts
	   vcTriEdge[2] = vcTriEdge[1] - vcTriEdge[0];
	   for (int j=0; j<3; j++) 
	   {
		  for (int k=0; k<3; k++) 
		  {
			 vcV = vcTriEdge[j].cross(vcA[k]);

			 this->tri_proj(v0, v1, v2, vcV, &fMin0, &fMax0);
			 this->obb_proj((*this), vcV, &fMin1, &fMax1);
         
			 if ( (fMax1 < fMin0) || (fMax0 < fMin1) )
				return true;
		  }
	   }
   
	   return true;
	} // intersects(Tri)
 
	// intersects ray, slaps method
	bool Obb::intersects(const Obb &obb) 
	{
	   float T[3];
   
	   // difference Vector4 between both obb
	   Vector4 vcD = obb.vcCenter - this->vcCenter;

	   float matM[3][3];   // B's axis in relation to A
	   float ra,           // radius A
			 rb,           // radius B
			 t;            // absolute values from T[]
   
	   // Obb A's axis as separation axis?
	   // ================================
	   // first axis vcA0
	   matM[0][0] = dot(this->vcA0, obb.vcA0);
	   matM[0][1] = dot(this->vcA0, obb.vcA1);
	   matM[0][2] = dot(this->vcA0, obb.vcA2);
	   ra   = this->fA0;
	   rb   = obb.fA0 * std::fabs(matM[0][0]) + 
			  obb.fA1 * std::fabs(matM[0][1]) + 
			  obb.fA2 * std::fabs(matM[0][2]);

	   T[0] = dot(vcD, this->vcA0);
	   t    = std::fabs(T[0]);
	   if(t > (ra + rb) ) 
		  return false;

	   // second axis vcA1
	   matM[1][0] = dot(this->vcA1, obb.vcA0);
	   matM[1][1] = dot(this->vcA1, obb.vcA1);
	   matM[1][2] = dot(this->vcA1, obb.vcA2);
	   ra   = this->fA1;
	   rb   = obb.fA0 * std::fabs(matM[1][0]) + 
			  obb.fA1 * std::fabs(matM[1][1]) + 
			  obb.fA2 * std::fabs(matM[1][2]);
	   T[1] = dot(vcD, this->vcA1);
	   t    = std::fabs(T[1]);
	   if(t > (ra + rb) ) 
		  return false;

	   // third axis vcA2
	   matM[2][0] = dot(this->vcA2, obb.vcA0);
	   matM[2][1] = dot(this->vcA2, obb.vcA1);
	   matM[2][2] = dot(this->vcA2, obb.vcA2);
	   ra   = this->fA2;
	   rb   = obb.fA0 * std::fabs(matM[2][0]) + 
			  obb.fA1 * std::fabs(matM[2][1]) + 
			  obb.fA2 * std::fabs(matM[2][2]);
	   T[2] = dot(vcD, this->vcA2);
	   t    = std::fabs(T[2]);
	   if(t > (ra + rb) ) 
		  return false;

	   // Obb B's axis as separation axis?
	   // ================================
	   // first axis vcA0
	   ra = this->fA0 * std::fabs(matM[0][0]) + 
			this->fA1 * std::fabs(matM[1][0]) + 
			this->fA2 * std::fabs(matM[2][0]);
	   rb = obb.fA0;
	   t = std::fabs( T[0]*matM[0][0] + T[1]*matM[1][0] + T[2]*matM[2][0] );
	   if(t > (ra + rb) )
		  return false;

	   // second axis vcA1
	   ra = this->fA0 * std::fabs(matM[0][1]) + 
			this->fA1 * std::fabs(matM[1][1]) + 
			this->fA2 * std::fabs(matM[2][1]);
	   rb = obb.fA1;
	   t = std::fabs( T[0]*matM[0][1] + T[1]*matM[1][1] + T[2]*matM[2][1] );
	   if(t > (ra + rb) )
		  return false;

	   // third axis vcA2
	   ra = this->fA0 * std::fabs(matM[0][2]) + 
			this->fA1 * std::fabs(matM[1][2]) + 
			this->fA2 * std::fabs(matM[2][2]);
	   rb = obb.fA2;
	   t = std::fabs( T[0]*matM[0][2] + T[1]*matM[1][2] + T[2]*matM[2][2] );
	   if(t > (ra + rb) )
		  return false;

	   // other candidates: cross products of axis:
	   // =========================================
	   // axis A0xB0
	   ra = this->fA1*std::fabs(matM[2][0]) + this->fA2*std::fabs(matM[1][0]);
	   rb = obb.fA1*std::fabs(matM[0][2]) + obb.fA2*std::fabs(matM[0][1]);
	   t = std::fabs( T[2]*matM[1][0] - T[1]*matM[2][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A0xB1
	   ra = this->fA1*std::fabs(matM[2][1]) + this->fA2*std::fabs(matM[1][1]);
	   rb = obb.fA0*std::fabs(matM[0][2]) + obb.fA2*std::fabs(matM[0][0]);
	   t = std::fabs( T[2]*matM[1][1] - T[1]*matM[2][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A0xB2
	   ra = this->fA1*std::fabs(matM[2][2]) + this->fA2*std::fabs(matM[1][2]);
	   rb = obb.fA0*std::fabs(matM[0][1]) + obb.fA1*std::fabs(matM[0][0]);
	   t = std::fabs( T[2]*matM[1][2] - T[1]*matM[2][2] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB0
	   ra = this->fA0*std::fabs(matM[2][0]) + this->fA2*std::fabs(matM[0][0]);
	   rb = obb.fA1*std::fabs(matM[1][2]) + obb.fA2*std::fabs(matM[1][1]);
	   t = std::fabs( T[0]*matM[2][0] - T[2]*matM[0][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB1
	   ra = this->fA0*std::fabs(matM[2][1]) + this->fA2*std::fabs(matM[0][1]);
	   rb = obb.fA0*std::fabs(matM[1][2]) + obb.fA2*std::fabs(matM[1][0]);
	   t = std::fabs( T[0]*matM[2][1] - T[2]*matM[0][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB2
	   ra = this->fA0*std::fabs(matM[2][2]) + this->fA2*std::fabs(matM[0][2]);
	   rb = obb.fA0*std::fabs(matM[1][1]) + obb.fA1*std::fabs(matM[1][0]);
	   t = std::fabs( T[0]*matM[2][2] - T[2]*matM[0][2] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB0
	   ra = this->fA0*std::fabs(matM[1][0]) + this->fA1*std::fabs(matM[0][0]);
	   rb = obb.fA1*std::fabs(matM[2][2]) + obb.fA2*std::fabs(matM[2][1]);
	   t = std::fabs( T[1]*matM[0][0] - T[0]*matM[1][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB1
	   ra = this->fA0*std::fabs(matM[1][1]) + this->fA1*std::fabs(matM[0][1]);
	   rb = obb.fA0 *std::fabs(matM[2][2]) + obb.fA2*std::fabs(matM[2][0]);
	   t = std::fabs( T[1]*matM[0][1] - T[0]*matM[1][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB2
	   ra = this->fA0*std::fabs(matM[1][2]) + this->fA1*std::fabs(matM[0][2]);
	   rb = obb.fA0*std::fabs(matM[2][1]) + obb.fA1*std::fabs(matM[2][0]);
	   t = std::fabs( T[1]*matM[0][2] - T[0]*matM[1][2] );
	   if( t > ra + rb )
		  return false;
   
	   // no separation axis found => intersection
	   return true;
	} // intersects(obb)
 
	/**
	 * Culls OBB to n sided frustrum. Normals pointing outwards.
	 * -> IN:  Plane   - array of planes building frustrum
	 *         int        - number of planes in array
	 *    OUT: CullResult::kVisible - obb totally inside frustrum
	 *         CullResult::kClipped - obb clipped by frustrum
	 *         CullResult::kCulled  - obb totally outside frustrum
	 */
	CullResult Obb::cull(const Plane *pPlanes, int nNumPlanes)
	{
	   Vector4 vN;
	   CullResult nResult = CullResult::kVisible;
	   float     fRadius, fTest;

	   // for all planes
	   for (int i=0; i<nNumPlanes; i++) 
	   {
		  // frustrum normals pointing outwards, we need inwards
		  vN = pPlanes[i].m_vcN * -1.0f;

		  // calculate projected box radius
		  fRadius = std::fabs(fA0 * dot(vN, vcA0)) 
				  + std::fabs(fA1 * dot(vN, vcA1))
				  + std::fabs(fA2 * dot(vN, vcA2));

		  // testvalue: (N*C - d) (#)
		  fTest = dot(vN, this->vcCenter) - pPlanes[i].m_fD;

		  // obb totally outside of at least one plane: (#) < -r
		  if (fTest < -fRadius)
			 return CullResult::kCulled;
		  // or maybe intersecting this plane?
		  else if (!(fTest > fRadius))
			 nResult = CullResult::kClipped;
		} // for

	   // if not culled then clipped or inside
	   return nResult;
	}
 
}